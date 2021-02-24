#include <graphics.h>
#include <types.h>
#include <thread>
#include <shaders.h>
#include <transform.h>
#include <utilities.h>
#include <buffers.h>
#include <theme.h>
#include <geometry.h>
#include <bufferview.h>
#include <chrono>
#include <thread>
#include <app.h>
#include <vector>
//NOTE: We do have access to glad should we keep it or imlement it?
#include <glad/glad.h>
#include <sstream>
#include <keyboard.h>

#include <x11_display.h>
#include <iostream>

//NOTE: Since we already modified fontstash source to reduce draw calls
//      we might as well embrace it
#define FONS_VERTEX_COUNT 8192
#define FONTSTASH_IMPLEMENTATION
#include <fontstash.h>

#define GLFONTSTASH_IMPLEMENTATION
#include <gl3corefontstash.h>
#include <unistd.h>

#define MODULE_NAME "Render"

#define TRANSITION_DURATION_SCROLL 0.15
#define TRANSITION_DURATION_JUMP   0.1

typedef enum{
    ViewportLineNumbers,
    ViewportText,
    ViewportFrame,
    ViewportAllView
}GLViewport;

typedef struct{
    Float fontSizeAtRenderCall;
    Float fontSizeAtDisplay;
    Float reduceScale;
    Float invReduceScale;
}FontMath;

typedef struct{
    FONScontext *fsContext;
    FONSsdfSettings sdfSettings;
    Shader shader;
    Shader cursorShader;
    int fontId;
    FontMath fontMath;
}OpenGLFont;

typedef struct{
    vec2f pMin;
    vec2f pMax;
    int valid;
    int pGlyph;
    vec2ui textPos;
    Float currentDif;
}OpenGLCursor;

typedef struct{
    //Data stuff
    Float *vertex;
    Float *colors;
    uint size;
    uint length;
    //OpenGL stuff
    uint vertexArray;
    uint vertexBuffer;
    uint colorBuffer;
}OpenGLBuffer;

typedef struct{
    WindowX11 *window;
    OpenGLFont font;
    OpenGLBuffer glQuadBuffer;
    OpenGLBuffer glLineBuffer;
    OpenGLCursor glCursor, glGhostCursor;
    int running, width, height;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
    Transform model;
}OpenGLState;

static OpenGLState GlobalGLState;

/*
* NOTE: IMPORTANT!
* Remenber that we render things in higher scale and than
* downscale with a projection, so if you want to specify 
* a rectangle or whatever in screen space [0, 0] x [width, height]
* you need to multiply the coordinates by the inverse scale:
*               font->fontMath.invReduceScale
*/

static Float ComputeTransformOf(Float x, Transform *transform){
    vec3f p(x, 0, 0);
    p = transform->Point(p);
    return p.x;
}

static Transform ComputeScreenToGLTransform(OpenGLState *state, Float lineHeight){
    OpenGLFont *font = &state->font;
    FontMath *fMath = &font->fontMath;
    Float scale = lineHeight / fMath->fontSizeAtRenderCall;
    return Scale(scale, scale, 1);
}


void OpenGLComputeProjection(vec2ui lower, vec2ui upper, Transform *transform){
    Float width = (Float)(upper.x - lower.x);
    Float height = (Float)(upper.y - lower.y);
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    *transform = Orthographic(0, width, height, 0, zNear, zFar);
    
}

static void ActivateViewportAndProjection(OpenGLState *state, BufferView *view,
                                          GLViewport target)
{
    uint x0 = 0, y0 = 0, w = 0, h = 0;
    OpenGLFont *font = &state->font;
    Geometry *geometry = &view->geometry;
    uint off = view->lineOffset * font->fontMath.reduceScale;
    switch(target){
        case ViewportLineNumbers:{
            x0 = geometry->lower.x;
            y0 = geometry->lower.y;
            w = off;
            h = geometry->upper.y - geometry->lower.y - font->fontMath.fontSizeAtDisplay;
            if(view->descLocation == DescriptionBottom){
                y0 += font->fontMath.fontSizeAtDisplay;
            }
        } break;
        case ViewportText:{
            //TODO: Should this remove the 1 line used for file bar?
            x0 = geometry->lower.x + off;
            y0 = geometry->lower.y;
            w = geometry->upper.x - geometry->lower.x - off;
            h = geometry->upper.y - geometry->lower.y;
        } break;
        case ViewportAllView:{
            x0 = geometry->lower.x;
            y0 = geometry->lower.y;
            w = geometry->upper.x - geometry->lower.x;
            h = geometry->upper.y - geometry->lower.y;
        } break;
        default:{
            AssertErr(0, "Target rendering does not contains a valid viewport");
        }
    }
    
    OpenGLComputeProjection(vec2ui(x0, y0), vec2ui(x0+w,y0+h), &state->projection);
    
    glViewport(x0, y0, w, h);
    glScissor(x0, y0, w, h);
    glEnable(GL_SCISSOR_TEST);
}

static Float ScreenToGL(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    Transform inv = Inverse(state->model);
    p = inv.Point(p);
    return p.x;
}

static Float GLToScreen(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    p = state->model.Point(p);
    return p.x;
}

static vec2ui ScreenToGL(vec2ui u, OpenGLState * state){
    vec3f p(u.x, u.y, 0);
    Transform inv = Inverse(state->model);
    p = inv.Point(p);
    return vec2ui((uint)p.x, (uint)p.y);
}

static void _OpenGLBufferInitialize(OpenGLBuffer *buffer, int n){
    buffer->vertex = AllocatorGetN(Float, 2 * n);
    buffer->colors = AllocatorGetN(Float, 4 * n);
    buffer->size = n;
    buffer->length = 0;
    
    glGenVertexArrays(1, &buffer->vertexArray);
    glBindVertexArray(buffer->vertexArray);
    
    glGenBuffers(1, &buffer->vertexBuffer);
    glGenBuffers(1, &buffer->colorBuffer);
}

static void _OpenGLBufferPushVertex(OpenGLBuffer *buffer, Float x, Float y, vec4f color){
    buffer->vertex[2 * buffer->length + 0] = x;
    buffer->vertex[2 * buffer->length + 1] = y;
    
    buffer->colors[4 * buffer->length + 0] = color.x;
    buffer->colors[4 * buffer->length + 1] = color.y;
    buffer->colors[4 * buffer->length + 2] = color.z;
    buffer->colors[4 * buffer->length + 3] = color.w;
    
    buffer->length ++;
}

static void _OpenGLStateInitialize(OpenGLState *state){
    int targetWidth = 1600;
    int targetHeight = 900;
    state->font.fsContext = nullptr;
    state->font.fontId = -1;
    state->window = nullptr;
    state->width = targetWidth;
    state->height = targetHeight;
    state->windowBounds = Bounds2f(vec2f(0, 0), vec2f(targetWidth, targetHeight));
    memset(&state->font.sdfSettings, 0x00, sizeof(FONSsdfSettings));
}

void OpenGLComputeCursorProjection(OpenGLState *state, OpenGLCursor *glCursor,
                                   Transform *transform, Float lineWidth,
                                   BufferView *view)
{
#if 1
    if(glCursor->valid){
        Scroll *scroll = &view->scroll;
        Transform model = state->scale * scroll->horizontal;
        Float dif = 0;
        Float cursorMaxX = ComputeTransformOf(glCursor->pMax.x, &model);
        Float cursorMinX = ComputeTransformOf(glCursor->pMin.x, &model);
        
        if(cursorMaxX > lineWidth){
            cursorMaxX = ComputeTransformOf(glCursor->pMax.x, &state->scale);
            dif = cursorMaxX - lineWidth;
            scroll->horizontal = Translate(-dif, 0, 0);
        }else if(cursorMinX < 0){
            dif = ComputeTransformOf(glCursor->pMin.x, &state->scale);
            scroll->horizontal = Translate(-dif, 0, 0);
        }
        
        //TODO: I'm unsure how to reset the view, this seems to work for now
        if(glCursor->textPos.y == 0){
            scroll->horizontal = Transform();
        }
        
        *transform = scroll->horizontal;
    }
#else
    *transform = Transform();
#endif
}

static void _OpenGLUpdateProjections(OpenGLState *state, int width, int height){
    BufferView *bufferView = AppGetActiveBufferView();
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    
    state->width = width;
    state->height = height;
    OpenGLFont *font = &state->font;
    state->projection = Orthographic(0.0f, width, height, 0.0f, zNear, zFar);
    state->scale = Scale(font->fontMath.reduceScale, 
                         font->fontMath.reduceScale, 1);
    
    Geometry geometry;
    geometry.lower = vec2ui(0, 0);
    geometry.upper = vec2ui(width, height);
    AppSetViewingGeometry(geometry, font->fontMath.fontSizeAtDisplay);
}

//TODO: Timing system.
double lastTime;
void WindowOnSizeChange(int width, int height){
    _OpenGLUpdateProjections(&GlobalGLState, width, height);
}

void WindowOnScroll(int is_up){ 
    int scrollRange = 5;
    BufferView *bufferView = AppGetActiveBufferView();
    BufferView_StartScrollViewTransition(bufferView, is_up ? -scrollRange : scrollRange,
                                         TRANSITION_DURATION_SCROLL);
    lastTime = GetElapsedTime();
    //BufferView_StartCursorTransition(bufferView, 100, TRANSITION_DURATION_JUMP);
#if 0
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    if(!is_up){
        BufferView_StartCursorTransition(bufferView, cursor.x + 100,
                                         TRANSITION_DURATION);
    }else{
        BufferView_StartCursorTransition(bufferView, Max(0, cursor.x - 100),
                                         TRANSITION_DURATION);
    }
#endif
}

void WindowOnClick(int x, int y){
    OpenGLFont *font = &GlobalGLState.font;
    vec2ui mouse = AppActivateBufferViewAt(x, y);
    
    //TODO: States
    BufferView *bufferView = AppGetActiveBufferView();
    int lineNo = BufferView_ComputeTextLine(bufferView, mouse.y);
    if(lineNo < 0) return;
    
    lineNo = Clamp(lineNo, 0, BufferView_GetLineCount(bufferView)-1);
    
    uint colNo = 0;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
    x = ScreenToGL(mouse.x, &GlobalGLState) - bufferView->lineOffset;
    if(x > 0){
        colNo = fonsComputeStringOffsetCount(GlobalGLState.font.fsContext,
                                             buffer->data, x);
        BufferView_CursorToPosition(bufferView, (uint)lineNo, colNo);
        BufferView_GhostCursorFollow(bufferView);
    }
}

void WinOnFocusChange(){
    KeyboardResetState();
}

int Font_SupportsCodepoint(int codepoint){
    int g = fonsGetGlyphIndex(GlobalGLState.font.fsContext, codepoint);
    return g != 0;
}

void RegisterInputs(WindowX11 *window){
    RegisterOnScrollCallback(window, WindowOnScroll);
    RegisterOnMouseClickCallback(window, WindowOnClick);
    RegisterOnSizeChangeCallback(window, WindowOnSizeChange);
    RegisterOnFocusChangeCallback(window, WinOnFocusChange);
}

void OpenGLFontSetup(OpenGLFont *font){
    uint filesize = 0;
    FontMath *fMath = nullptr;
    char *fontfileContents = nullptr;
    
    //TODO: These should not be files
    const char *v0 = "/home/felipe/Documents/Mayhem/shaders/text.v.glsl";
    const char *f0 = "/home/felipe/Documents/Mayhem/shaders/text.f.glsl";
    const char *v1 = "/home/felipe/Documents/Mayhem/shaders/cursor.v.glsl";
    const char *f1 = "/home/felipe/Documents/Mayhem/shaders/cursor.f.glsl";
    
    const char *ttf = "/home/felipe/Documents/4coder/fonts/liberation-mono.ttf";
    uint vertex    = Shader_CompileFile(v0, SHADER_TYPE_VERTEX);
    uint fragment  = Shader_CompileFile(f0, SHADER_TYPE_FRAGMENT);
    uint cvertex   = Shader_CompileFile(v1, SHADER_TYPE_VERTEX);
    uint cfragment = Shader_CompileFile(f1, SHADER_TYPE_FRAGMENT);
    
    Shader_Create(font->shader, vertex, fragment);
    Shader_Create(font->cursorShader, cvertex, cfragment);
    
    font->sdfSettings.sdfEnabled = 0;
    font->fsContext = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
    
    fontfileContents = GetFileContents(ttf, &filesize);
    AssertA(fontfileContents != nullptr && filesize > 0, "Failed to load font");
    
    font->fontId = fonsAddFontSdfMem(font->fsContext, "Default", 
                                     (uint8 *)fontfileContents, filesize, 0,
                                     font->sdfSettings);
    AssertA(font->fontId != FONS_INVALID, "Failed to create font");
    fMath = &font->fontMath;
    fMath->fontSizeAtRenderCall = 65;
    fMath->fontSizeAtDisplay = 18; // TODO: user
    
    fMath->reduceScale = fMath->fontSizeAtDisplay / fMath->fontSizeAtRenderCall;
    fMath->invReduceScale = 1.0 / fMath->reduceScale;
}

void OpenGLInitialize(OpenGLState *state){
    DEBUG_MSG("Initializing OpenGL graphics\n");
    OpenGLBuffer *quad  = &state->glQuadBuffer;
    OpenGLBuffer *lines = &state->glLineBuffer;
    int width = state->width;
    int height = state->height;
    
    InitializeX11();
    SetSamplesX11(16);
    SetOpenGLVersionX11(3, 3);
    state->window = CreateWindowX11(width, height, "Cody - 0.0.1");
    
    AssertErr(gladLoadGL() != 0, "Failed to load OpenGL pointers");
    GLenum error = glGetError();
    AssertErr(error == GL_NO_ERROR, "Failed to setup opengl context");
    
    _OpenGLBufferInitialize(quad, 1024);
    _OpenGLBufferInitialize(lines, 1024);
    
    AppInitialize();
    
    SwapIntervalX11(state->window, 0);
    RegisterInputs(state->window);
    
}

void Graphics_QuadPush(OpenGLState *state, vec2ui left, vec2ui right, vec4f color){
    Float x0 = left.x, y0 = left.y, x1 = right.x, y1 = right.y;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    _OpenGLBufferPushVertex(quad, x0, y0, color);
    _OpenGLBufferPushVertex(quad, x1, y1, color);
    _OpenGLBufferPushVertex(quad, x1, y0, color);
    
    _OpenGLBufferPushVertex(quad, x0, y0, color);
    _OpenGLBufferPushVertex(quad, x0, y1, color);
    _OpenGLBufferPushVertex(quad, x1, y1, color);
}

void Graphics_LinePush(OpenGLState *state, vec2ui p0, vec2ui p1, vec4f color){
    OpenGLBuffer *lines = &state->glLineBuffer;
    _OpenGLBufferPushVertex(lines, p0.x, p0.y, color);
    _OpenGLBufferPushVertex(lines, p1.x, p1.y, color);
}

void Graphics_QuadFlush(OpenGLState *state, int blend = 1){
    OpenGLBuffer *quad = &state->glQuadBuffer;
    if(quad->length > 0){
        if(blend){
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        glBindVertexArray(quad->vertexArray);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, quad->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, quad->length * 2 * sizeof(Float),
                     quad->vertex, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, quad->colorBuffer);
        glBufferData(GL_ARRAY_BUFFER, quad->length * 4 * sizeof(Float),
                     quad->colors, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glDrawArrays(GL_TRIANGLES, 0, quad->length);
        
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }
    quad->length = 0;
}

void Graphics_LineFlush(OpenGLState *state, int blend = 1){
    OpenGLBuffer *lines = &state->glLineBuffer;
    if(lines->length > 0){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindVertexArray(lines->vertexArray);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, lines->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, lines->length * 2 * sizeof(Float), lines->vertex, 
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, lines->colorBuffer);
        glBufferData(GL_ARRAY_BUFFER, lines->length * 4 * sizeof(Float), lines->colors, 
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glDrawArrays(GL_LINES, 0, lines->length);
        
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }
    lines->length = 0;
}

void OpenGLRenderEmptyCursorRect(OpenGLState *state, vec2f lower, vec2f upper, vec4f color, uint thickness)
{
    Float x0 = lower.x, y0 = lower.y, x1 = upper.x, y1 = upper.y;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    if(quad->length + 24 > quad->size){
        Graphics_QuadFlush(state);
    }
    uint w = thickness;
    Graphics_QuadPush(state, vec2ui(x0+0, y0+0), vec2ui(x0+w, y1+0), color);
    Graphics_QuadPush(state, vec2ui(x0+0, y1+0), vec2ui(x1+w, y1+w), color);
    Graphics_QuadPush(state, vec2ui(x1+0, y1+0), vec2ui(x1+w, y0+w), color);
    Graphics_QuadPush(state, vec2ui(x1+w, y0+w), vec2ui(x0+0, y0+0), color);
}

void OpenGLComputeCursor(OpenGLState *state, OpenGLCursor *glCursor, 
                         Buffer *buffer, vec2ui cursor, Float cursorOffset, 
                         Float baseHeight, vec2ui visibleLines)
{
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
    int utf8Len = 1;
    int pUtf8Len = 1;
    char *ptr = buffer->data;
    
    glCursor->pGlyph = -1;
    glCursor->valid = 0;
    glCursor->textPos = cursor;
    
    if(cursor.x >= visibleLines.x && cursor.x <= visibleLines.y){
        glCursor->valid = 1;
        if(cursor.y > 0){
            rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y-1, &utf8Len);
            x0 = fonsComputeStringAdvance(state->font.fsContext, ptr,
                                          rawp+utf8Len, &glCursor->pGlyph);
        }
        
        rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, &pUtf8Len);
        x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                           pUtf8Len, &glCursor->pGlyph);
        
        y0 = (cursor.x - visibleLines.x) * state->font.fontMath.fontSizeAtRenderCall;
        y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
        y0 += baseHeight;
        y1 += baseHeight;
        
        x0 += cursorOffset;
        x1 += cursorOffset;
        
        glCursor->pMin = vec2f(x0, y0);
        glCursor->pMax = vec2f(x1, y1);
    }
}

void OpenGLRenderCursor(OpenGLState *state, OpenGLCursor *glCursor, vec4f color,
                        uint thickness, int isActive)
{
    Float x0 = glCursor->pMin.x, x1 = glCursor->pMax.x;
    Float y0 = glCursor->pMin.y, y1 = glCursor->pMax.y;
    if(isActive){
        Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), color);
    }else{
        OpenGLRenderEmptyCursorRect(state, vec2f(x0, y0), vec2f(x1, y1), 
                                    color, thickness);
    }
}

vec4f OpenGLRenderCursor(OpenGLState *state, Buffer *buffer, vec2ui cursor,
                         Float cursorOffset, Float baseHeight, vec2ui visibleLines,
                         int *prevGlyph, int isActive, vec4f color, uint thickness)
{
    int nId = 0;
    int previousGlyph = -1;
    char *ptr = buffer->data;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
    int utf8Len = 1;
    int pUtf8Len = 1;
    
    if(cursor.y > 0){
        rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y-1, &utf8Len);
        //printf("Rawp: %u, len: %d Y: %u\n", rawp, utf8Len, cursor.y);
        x0 = fonsComputeStringAdvance(state->font.fsContext, ptr,
                                      rawp+utf8Len, &previousGlyph);
    }
    
    //if(buffer->count > 1 && cursor.y > 1) DebugBreak();
    rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, &pUtf8Len);
    //printf("Rawp: %u, len: %d Y: %u, %s\n", rawp, pUtf8Len, cursor.y, &ptr[rawp]);
    x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                       pUtf8Len, &previousGlyph);
    
    y0 = (cursor.x - visibleLines.x) * state->font.fontMath.fontSizeAtRenderCall;
    y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
    y0 += baseHeight;
    y1 += baseHeight;
    
    x0 += cursorOffset;
    x1 += cursorOffset;
    
    if(isActive){
        if(quad->length + 6 > quad->size){
            Graphics_QuadFlush(state);
        }
        Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), color);
    }else{
        OpenGLRenderEmptyCursorRect(state, vec2f(x0, y0), vec2f(x1, y1), 
                                    color, thickness);
    }
    *prevGlyph = previousGlyph;
    
    return vec4f(x0, y0, x1, y1);
}

void OpenGLRenderLineNumber(BufferView *view, OpenGLFont *font,
                            Float &x, Float &y, uint lineNr, char linen[32],
                            Theme *theme)
{
    if(view->renderLineNbs){
        int previousGlyph = -1;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        int spacing = DigitCount(BufferView_GetLineCount(view));
        int ncount = DigitCount(lineNr+1);
        memset(linen, ' ', spacing-ncount);
        snprintf(&linen[spacing-ncount], 32, "%u", lineNr+1);
        
        vec4i col = GetUIColor(theme, UILineNumbers);
        if(lineNr == cursor.x){
            col = GetUIColor(theme, UILineNumberHighlighted);
        }
        
        if(!view->isActive){
            col.w *= 0.2;
        }
        
        x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(), 
                                    linen, NULL, &previousGlyph);
    }
}

void OpenGLRenderAllLineNumbers(OpenGLState *state, BufferView *view, Theme *theme){
    if(view->renderLineNbs){
        Float x0 = 0;
        Float x = x0;
        Float y = 0;
        char linen[32];
        OpenGLFont *font = &state->font;
        vec2ui lines = BufferView_GetViewRange(view);
        
        fonsClearState(font->fsContext);
        fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
        fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        
        glUseProgram(font->shader.id);
        Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
        Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);
        
        for(int i = lines.x; i < lines.y; i++){
            OpenGLRenderLineNumber(view, font, x, y, i, linen, theme);
            x = x0;
            y += font->fontMath.fontSizeAtRenderCall;
        }
        
        fonsStashFlush(font->fsContext);
    }
}

void OpenGLRenderLine(BufferView *view, OpenGLState *state,
                      Float &x, Float &y, uint lineNr)
{
    
    int previousGlyph = -1;
    OpenGLFont *font = &state->font;
    Buffer *buffer = BufferView_GetBufferAt(view, lineNr);
    
    NestPoint *start = view->startNest;
    NestPoint *end   = view->endNest;
    uint it = Max(view->startNestCount, view->endNestCount);
    
    //TODO: This is where we would wrap?
    if(buffer->taken > 0){
        uint pos = 0;
        uint at = 0;
        for(int i = 0; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            while(pos < token->position){
                char v = buffer->data[pos];
                if(v == '\r' || v == '\n'){
                    pos++;
                    continue;
                }
                
                if(v == '\t' || v == ' '){
                    x += fonsComputeStringAdvance(font->fsContext, " ", 1, &previousGlyph);
                    pos ++;
                    continue;
                }
                
                if(v == 0){
                    return;
                }
                
                printf("Position %u  ==> Token %u, char \'%c\'\n", pos, token->position, v); 
                printf("Data : %s, taken: %u\n", buffer->data, buffer->taken);
                BreakIf(0, "Invalid character");
                AssertA(0, "Invalid character");
            }
            
            if(token->identifier == TOKEN_ID_SPACE){
                //TODO: Find out spacing required for this token, do we need tho?
                
            }else if(token->position < buffer->taken){
                char *p = &buffer->data[token->position];
                char *e = &buffer->data[token->position + token->size];
                vec4i col = GetColor(defaultTheme, token->identifier);
                if(BufferView_CursorNestIsValid(view)){
                    for(uint f = 0; f < it; f++){
                        if(f < view->startNestCount){
                            if(start[f].valid){ // is valid
                                if((lineNr == start[f].position.x && 
                                    i == start[f].position.y))
                                {
                                    col = GetNestColor(defaultTheme, start[f].id,
                                                       start[f].depth);
                                    break;
                                }
                            }
                        }
                        
                        if(f < view->endNestCount){
                            if(end[f].valid){ // is valid
                                if((lineNr == end[f].position.x && 
                                    i == end[f].position.y))
                                {
                                    col = GetNestColor(defaultTheme, end[f].id,
                                                       start[f].depth);
                                    break;
                                }
                            }
                        }
                    }
                }
                
                pos += token->size;
                x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                            p, e, &previousGlyph);
                
            }
        }
    }
}

void _Graphics_RenderTextBlock(OpenGLState *state, BufferView *view, Float baseHeight,
                               Transform *projection, Transform *model, vec2ui lines)
{
    Float x0 = 2.0f;
    Float x = x0;
    Float y = baseHeight;
    char linen[32];
    OpenGLFont *font = &state->font;
    
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->model.m);
    
    x = x0;
    y = baseHeight;
    for(int i = lines.x; i < lines.y; i++){
        OpenGLRenderLine(view, state, x, y, i);
        x = x0;
        y += font->fontMath.fontSizeAtRenderCall;
    }
    
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
}

void _Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                    Transform *model, Transform *projection)
{
    int pGlyph = -1;
    OpenGLFont *font = &state->font;
    OpenGLCursor *cursor = &state->glCursor;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor->textPos.x);
    
    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
    
    glDisable(GL_BLEND);
    OpenGLRenderCursor(state, &state->glCursor, GetUIColorf(defaultTheme, UICursor),
                       4, bufferView->isActive);
    
    if(state->glGhostCursor.valid){
        Float g = 0.5;
        Float a = 0.6;
        OpenGLRenderCursor(state, &state->glGhostCursor, vec4f(g, g, g, a), 2, 0);
    }
    
    if(bufferView->isActive){
        /* Redraw whatever character we are on top of */
        // TODO: Why do we need to flush the quad buffer here in order to render
        //       the last character?
        Graphics_QuadFlush(state);
        if(cursor->textPos.y < buffer->count){
            vec2f p = state->glCursor.pMin;
            pGlyph = state->glCursor.pGlyph;
            int len = 0;
            uint rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor->textPos.y, &len);
            char *chr = &buffer->data[rawp];
            if(*chr != '\t'){
                glEnable(GL_BLEND);
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
                
                glUseProgram(font->shader.id);
                Shader_UniformMatrix4(font->shader, "projection", &projection->m);
                Shader_UniformMatrix4(font->shader, "modelView", &model->m);
                
                float f = fonsStashMultiTextColor(font->fsContext, p.x, p.y, 
                                                  glfonsRGBA(0, 0, 0, 255),
                                                  chr, chr+len, &pGlyph);
                fonsStashFlush(font->fsContext);
            }
        }
    }else{
        Graphics_QuadFlush(state);
    }
}

void Graphics_RenderTextBlock(OpenGLState *state, BufferView *bufferView,
                              Float baseHeight, Transform *projection)
{
    vec2ui lines;
    lines = BufferView_GetViewRange(bufferView);
    _Graphics_RenderTextBlock(state, bufferView, baseHeight,
                              projection, &state->model, lines);
}

void Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                   Transform *projection)
{
    //Transform translate = state->scale * Translate(0, 18, 0);
    _Graphics_RenderCursorElements(state, bufferView, &state->model, projection);
}

void Graphics_RenderScopeSections(OpenGLState *state, BufferView *view, Float lineSpan,
                                  Transform *projection, Transform *model, Theme *theme)
{
    if(view->isActive){
        OpenGLFont *font = &state->font;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        vec2ui visibleLines = BufferView_GetViewRange(view);
        
        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        
        if(BufferView_CursorNestIsValid(view)){
            NestPoint *start = view->startNest;
            NestPoint *end   = view->endNest;
            Float minY = 0;
            Float maxY = view->currentMaxRange * font->fontMath.fontSizeAtRenderCall;
            struct _Quad{
                vec2ui left, right;
                vec4f color;
                int depth;
            };
            
            std::vector<_Quad> quads;
            
            if(view->descLocation == DescriptionTop){
                minY = font->fontMath.fontSizeAtRenderCall;
                maxY += font->fontMath.fontSizeAtRenderCall;
            }
            
            uint it = view->startNestCount;
            for(uint f = 0; f < it; f++){
                NestPoint v = start[f];
                if(v.valid && v.id == TOKEN_ID_BRACE_OPEN){ // valid
                    NestPoint e = end[v.comp];
                    Buffer *bStart = BufferView_GetBufferAt(view, v.position.x);
                    Buffer *bEnd   = BufferView_GetBufferAt(view, e.position.x);
                    
                    uint l0 = AppComputeLineIndentLevel(bStart);
                    uint l1 = AppComputeLineIndentLevel(bEnd);
                    
                    // only do it for aligned stuff
                    if(l0 == l1 && e.position.x > v.position.x + 1){
                        // check if their non empty token match position
                        vec2ui p0(0, 0);
                        vec2ui p1(0, 0);
                        uint n = Max(bStart->tokenCount, bEnd->tokenCount);
                        for(uint i = 0; i < n; i++){
                            if(i < bStart->tokenCount && p0.y == 0){
                                Token *t = &bStart->tokens[i];
                                if(t->identifier != TOKEN_ID_SPACE){
                                    p0.x = t->position;
                                    p0.y = 1;
                                }
                            }
                            
                            if(i < bEnd->tokenCount && p1.y == 0){
                                Token *t = &bEnd->tokens[i];
                                if(t->identifier != TOKEN_ID_SPACE){
                                    p1.x = t->position;
                                    p1.y = 1;
                                }
                            }
                            
                            if(p1.y != 0 && p0.y != 0) break;
                        }
                        
                        if((p1.y == 0 || p0.y == 0) || (p1.x != p0.x)){
                            continue;
                        }
                        
                        // looks like this thing needs rendering
                        Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
                        int pGlyph = -1;
                        char *p = &bStart->data[0];
                        
                        //TODO: UTF-8
                        x0 = fonsComputeStringAdvance(font->fsContext, p, p0.x, &pGlyph);
                        x1 = fonsComputeStringAdvance(font->fsContext, &p[p0.x], 1, &pGlyph);
                        
                        Float topX = x0 + 0.5f * x1;
                        Float botX = topX;
                        y0 = ((Float)v.position.x - (Float)visibleLines.x + 2) *
                            font->fontMath.fontSizeAtRenderCall;
                        
                        y1 = y0 + (e.position.x - v.position.x - 1) *
                            font->fontMath.fontSizeAtRenderCall;
                        
                        y0 = Max(y0, minY);
                        y1 = Min(y1, maxY);
                        
                        Graphics_LinePush(state, vec2ui(topX, y0), vec2ui(botX, y1),
                                          vec4f(0.5,0.5,0.5, 0.18));
                    }
                    
                    // render the quads no matter what
                    Float qy0 = 0, qy1 = 0, qx0 = 0, qx1 = 0;
                    qy0 = ((Float)v.position.x - (Float)visibleLines.x + 1) *
                        font->fontMath.fontSizeAtRenderCall;
                    
                    qy1 = qy0 + (e.position.x - v.position.x + 1) *
                        font->fontMath.fontSizeAtRenderCall;
                    
                    //TODO: Review this, this adds spacing for initialization of the scoped
                    //      quads
                    //qx0 = view->lineOffset;
                    qx1 = qx0 + lineSpan;
                    
                    qy0 = Max(qy0, minY);
                    qy1 = Min(qy1, maxY);
                    
                    vec4f color = GetNestColorf(theme, TOKEN_ID_SCOPE,
                                                v.depth);
                    quads.push_back({
                                        .left = vec2ui(qx0, qy0),
                                        .right = vec2ui(qx1, qy1),
                                        .color = color,
                                        .depth = v.depth
                                    });
                }
            }
            
            int quadLen = quads.size();
            short n = GetBackTextCount(theme);
            
            if(quadLen > 0 && n == 1){
                _Quad quad = quads.at(quadLen-1);
                Graphics_QuadPush(state, quad.left, quad.right,
                                  quad.color);
            }else{
                for(int i = quadLen - 1; i >= 0; i--){
                    _Quad quad = quads.at(i);
                    Graphics_QuadPush(state, quad.left, quad.right,
                                      quad.color);
                }
            }
            
            Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
            Graphics_QuadFlush(state, 0);
        }
        
        Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
        //Float g = 0.8705882; //TODO: add this to theme
        vec4f col = GetUIColorf(theme, UICursorLineHight);
        Float cy0 = ((Float)cursor.x - (Float)visibleLines.x + 1) *
            font->fontMath.fontSizeAtRenderCall;
        Float cy1 = cy0 + font->fontMath.fontSizeAtRenderCall;
        Graphics_QuadPush(state, vec2ui(0, cy0), 
                          vec2ui(lineSpan, cy1), 
                          col
                          //vec4f(g, g, 2 * g, 1.0)
                          //vec4f(g, g, g, 0.2)
                          );
        
        Graphics_QuadFlush(state, 0);
        
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
        Graphics_LineFlush(state);
        
    }
}

void Graphics_RenderFrame(OpenGLState *state, BufferView *view,
                          Transform *projection, Float lineSpan, Theme *theme)
{
    OpenGLFont *font = &state->font;
    Geometry *geometry = &view->geometry;
    
    int dummyGlyph = -1;
    vec2ui l = vec2ui(0, 0);
    vec2ui u = ScreenToGL(geometry->upper - geometry->lower, state);
    
    // Render the file description of this buffer view
    // TODO: states
    char desc[256];
    Float pc = BufferView_GetDescription(view, desc, sizeof(desc));
    
    glDisable(GL_BLEND);
    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
    
    vec2ui a1(u.x, u.y + font->fontMath.fontSizeAtRenderCall);
    vec2ui a0(l.x, u.y - font->fontMath.fontSizeAtRenderCall);
    
    //vec3f col = ColorRGB(theme->backgroundColor);
    //vec3f col(0.6);
    Float a = 0.12;
    Float g = 0.8705882; //TODO: add this to theme
    vec3f col = vec3f(g, g, 2 * g);
    
    if(view->descLocation == DescriptionTop){
        a0 = vec2ui(l.x, l.y);
        a1 = vec2ui(u.x, l.y + font->fontMath.fontSizeAtRenderCall);
    }else if(view->descLocation != DescriptionBottom){
        AssertA(0, "Invalid description position");
    }
    
    Graphics_QuadPush(state, a0, a1, vec4f(col.x, col.y, col.z, a));
    
    a = 0.07;
    col = vec3f(2 * g, 2 * g, g);
    vec2f b0 = vec2f((Float)a0.x + (Float)(a1.x - a0.x) * pc, (Float)a0.y);
    Graphics_QuadPush(state, vec2ui((uint)b0.x, (uint)b0.y),
                      a1, vec4f(col.x, col.y, col.z, a));
    
    Graphics_QuadFlush(state);
    
    
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);
    
    Float x = a0.x;
    Float y = a0.y;
    vec4i c = GetColor(theme, TOKEN_ID_DATATYPE);
    fonsStashMultiTextColor(font->fsContext, x, y, c.ToUnsigned(),
                            desc, NULL, &dummyGlyph);
    
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
    
    if(view->isActive){
        //vec4f color(0.78, 0.123, 0.1, 0.6); // TODO: state
        vec4f c = GetUIColorf(theme, UIBorder);
        vec4f color = vec4f(c.x, c.y, c.z, 0.7);
        
        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
        
#if 0
        int w = 2;
        vec2ui p = vec2ui(0, 0);
        vec2ui s = geometry->upper - geometry->lower;
        vec2ui k0, k1;
        k0 = ScreenToGL(vec2ui(p.x + w, s.y), state);
        Graphics_QuadPush(state, p, k0, color);
        
        k0 = ScreenToGL(vec2ui(s.x, p.y + w), state);
        Graphics_QuadPush(state, l, k0, color);
        
        
        k0 = ScreenToGL(vec2ui(s.x-w, l.y), state);
        k1 = ScreenToGL(vec2ui(s.x, s.y-w), state);
        Graphics_QuadPush(state, k0, k1, color);
        
        k0 = ScreenToGL(vec2ui(l.x, s.y-w), state);
        k1 = ScreenToGL(vec2ui(u.x, s.y), state);
        Graphics_QuadPush(state, k0, k1, color);
        
        Graphics_QuadFlush(state);
#endif
#if 0
        Graphics_LinePush(state, l, vec2ui(l.x, u.y), color); // left line
        Graphics_LinePush(state, vec2ui(l.x, u.y), u, color); // top line
        Graphics_LinePush(state, u, vec2ui(u.x, l.y), color);
        Graphics_LinePush(state, vec2ui(u.x, l.y), l, color);
        Graphics_LineFlush(state);
#endif
    }
}

int Graphics_RenderBufferView(BufferView *view, Theme *theme, Float dt){
    Float ones[] = {1,1,1,1};
    char linen[32];
    OpenGLState *state = &GlobalGLState;
    OpenGLFont *font = &GlobalGLState.font;
    Geometry geometry = view->geometry;
    
    Float originalScaleWidth = (geometry.upper.x - geometry.lower.x) * font->fontMath.invReduceScale;
    Transform translate;
    
    vec4f backgroundColor = GetUIColorf(theme, UIBackground);
    vec4f backgroundLineNColor = GetUIColorf(theme, UILineNumberBackground);
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };
    Float fcolLN[] = { backgroundLineNColor.x, backgroundLineNColor.y,
        backgroundLineNColor.z, backgroundLineNColor.w };
    
    int is_animating = 0;
    vec2ui cursor = BufferView_GetCursorPosition(view);
    vec2ui visibleLines = BufferView_GetViewRange(view);
    Buffer *cursorBuffer = BufferView_GetBufferAt(view, cursor.x);
    
    int n = snprintf(linen, sizeof(linen), "%u ", BufferView_GetLineCount(view));
    int pGlyph = -1;
    if(view->renderLineNbs){
        view->lineOffset = fonsComputeStringAdvance(font->fsContext, linen, n, &pGlyph);
    }else{
        view->lineOffset = fonsComputeStringAdvance(font->fsContext, " ",
                                                    1, &pGlyph) * 0.5;
    }
    
    ActivateViewportAndProjection(state, view, ViewportAllView);
    
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, ones);
    Float baseHeight = 0;
    if(view->descLocation == DescriptionTop){
        baseHeight = font->fontMath.fontSizeAtRenderCall;
    }
    
    // Sets the alpha for the current view rendering stages, makes the dimm effect
    SetAlpha(view->isActive ? 0 : 1);
    
    // Decouple the line numbering for the text viewing, this allows for horizontal
    // scrolling to not affect line numbers.
    
    ActivateViewportAndProjection(state, view, ViewportLineNumbers);
    glClearBufferfv(GL_COLOR, 0, fcolLN);
    
    OpenGLRenderAllLineNumbers(state, view, theme);
    
    geometry.lower.x += view->lineOffset * font->fontMath.reduceScale;
    Float width = geometry.upper.x - geometry.lower.x;
    Float scaledWidth = width * font->fontMath.invReduceScale;
    
    OpenGLComputeCursor(state, &state->glCursor, cursorBuffer,
                        cursor, 0, baseHeight, visibleLines);
    
    OpenGLComputeCursorProjection(state, &state->glCursor, &translate, width, view);
    state->model = state->scale * translate;
    
    if(view->isActive){
        cursor = BufferView_GetGhostCursorPosition(view);
        cursorBuffer = BufferView_GetBufferAt(view, cursor.x);
        OpenGLComputeCursor(state, &state->glGhostCursor, cursorBuffer,
                            cursor, 0, baseHeight, visibleLines);
    }else{
        state->glGhostCursor.valid = 0;
    }
    
    ActivateViewportAndProjection(state, view, ViewportText);
    
    // render alignment and scoped quads first to not need to blend this thing
    Graphics_RenderScopeSections(state, view, scaledWidth, &state->projection,
                                 &state->model, theme);
    
    if(!BufferView_IsAnimating(view)){
        Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
        Graphics_RenderCursorElements(state, view, &state->projection);
    }else{
        vec2ui range;
        vec2ui cursorAt;
        Transform translate;
        Transform model;
        Transition tr = BufferView_GetTransitionType(view);
        int manual = 1;
        
        switch(tr){
            case TransitionCursor:{
                BufferView_GetCursorTransition(view, dt, &range, &cursorAt, &translate);
            } break;
            case TransitionScroll:{
                BufferView_GetScrollViewTransition(view, dt, &range, &cursorAt, &translate);
            } break;
            case TransitionNumbers:{
                BufferView_GetNumbersShowTransition(view, dt);
                manual = 0;
            } break;
            
            default:{}
        }
        if(manual){
            model = state->scale * translate;
            cursorBuffer = BufferView_GetBufferAt(view, cursorAt.x);
            
            OpenGLComputeCursor(state, &state->glCursor, cursorBuffer, cursorAt,
                                0, baseHeight, range);
            
            _Graphics_RenderTextBlock(state, view, baseHeight, &state->projection,
                                      &model, range);
            _Graphics_RenderCursorElements(state, view, &model, &state->projection);
        }else{
            Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
            Graphics_RenderCursorElements(state, view, &state->projection);
        }
        is_animating = 1;
    }
    
    ActivateViewportAndProjection(state, view, ViewportAllView);
    Graphics_RenderFrame(state, view, &state->projection, originalScaleWidth, theme);
    
    glDisable(GL_SCISSOR_TEST);
    return is_animating;
}

/*
* TODO: It is interesting to attempt to render 1 +2pages and allow
*       for a translation matrix. It might allow us to better represent
*       transitions and the viewing interface? Also it allows us to by-pass
*       the 'translate at end of file' issue we have right now.
*/
void OpenGLEntry(){
    Float ones[] = {1,1,1,1};
    BufferView *bufferView = AppGetActiveBufferView();
    OpenGLFont *font = &GlobalGLState.font;
    
    _OpenGLStateInitialize(&GlobalGLState);
    OpenGLInitialize(&GlobalGLState);
    OpenGLFontSetup(&GlobalGLState.font);
    
    _OpenGLUpdateProjections(&GlobalGLState, GlobalGLState.width, 
                             GlobalGLState.height);
    
    BufferView_CursorTo(bufferView, 0);
    lastTime = GetElapsedTime();
    while(!WindowShouldCloseX11(GlobalGLState.window)){
        double currTime = GetElapsedTime();
        double dt = currTime - lastTime;
        int animating = 0;
        Float width = GlobalGLState.width;
        Float height = GlobalGLState.height;
        
        double fps = 1.0 / dt;
        
        lastTime = currTime;
        AppUpdateViews();
        
        int c = AppGetBufferViewCount();
        for(int i = 0; i < c; i++){
            BufferView *view = AppGetBufferView(i);
            BufferView_UpdateCursorNesting(view);
            animating |= Graphics_RenderBufferView(view, defaultTheme, dt);
        }
        
        SwapBuffersX11(GlobalGLState.window);
        if(animating){
            PoolEventsX11();
        }else{
            WaitForEventsX11();
        }
        
        //glfwWaitEvents();
        //usleep(10000);
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    DestroyWindowX11(GlobalGLState.window);
    TerminateX11();
    
    GlobalGLState.running = 0;
    GlobalGLState.window = nullptr;
}

int Graphics_IsRunning(){
    return GlobalGLState.running;
}

void Graphics_Initialize(){
    // TODO: Refactor
    OpenGLEntry();
#if 0
    if(GlobalGLState.running == 0){
        GlobalGLState.running = 1;
        std::thread(OpenGLEntry).detach();
    }
#endif
}
