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

#define TRANSITION_DURATION_SCROLL 0.2
#define TRANSITION_DURATION_JUMP   0.1

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
    int running, width, height;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
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

static Transform ComputeScreenToGLTransform(OpenGLState *state, Float lineHeight){
    OpenGLFont *font = &state->font;
    FontMath *fMath = &font->fontMath;
    Float scale = lineHeight / fMath->fontSizeAtRenderCall;
    return Scale(scale, scale, 1);
}

static Float ScreenToGL(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    Transform inv = Inverse(state->scale);
    p = inv.Point(p);
    return p.x;
}

static vec2ui ScreenToGL(vec2ui u, OpenGLState * state){
    vec3f p(u.x, u.y, 0);
    Transform inv = Inverse(state->scale);
    p = inv.Point(p);
    return vec2ui((uint)p.x, (uint)p.y);
}

static void _OpenGLBufferInitialize(OpenGLBuffer *buffer, int n){
    buffer->vertex = AllocatorGetN(Float, n);
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

void OpenGLComputeProjection(vec2ui lower, vec2ui upper, Transform *transform){
    Float width = (Float)(upper.x - lower.x);
    Float height = (Float)(upper.y - lower.y);
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    *transform = Orthographic(0, width, height, 0, zNear, zFar);
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

void WindowOnSizeChange(int width, int height){
    _OpenGLUpdateProjections(&GlobalGLState, width, height);
}

void WindowOnScroll(int is_up){
    int scrollRange = 6;
    BufferView *bufferView = AppGetActiveBufferView();
    BufferView_StartScrollViewTransition(bufferView, is_up ? -scrollRange : scrollRange,
                                         TRANSITION_DURATION_SCROLL);
    
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

void Graphics_QuadFlush(OpenGLState *state){
    OpenGLBuffer *quad = &state->glQuadBuffer;
    if(quad->length > 0){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(quad->vertexArray);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, quad->vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, quad->length * 2 * sizeof(Float), quad->vertex, 
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, quad->colorBuffer);
        glBufferData(GL_ARRAY_BUFFER, quad->length * 4 * sizeof(Float), quad->colors, 
                     GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        
        glDrawArrays(GL_TRIANGLES, 0, quad->length);
        
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
    }
    quad->length = 0;
}

void Graphics_LineFlush(OpenGLState *state){
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

void OpenGLRenderLine(BufferView *view, OpenGLFont *font,
                      Float &x, Float &y, uint lineNr)
{
    char linen[32];
    int previousGlyph = -1;
    
    Buffer *buffer = BufferView_GetBufferAt(view, lineNr);
    int spacing = DigitCount(BufferView_GetLineCount(view));
    int ncount = DigitCount(lineNr+1);
    vec4ui *start = view->startNest;
    vec4ui *end   = view->endNest;
    uint it = Max(view->startNestCount, view->endNestCount);
    
    memset(linen, ' ', spacing-ncount);
    snprintf(&linen[spacing-ncount], 32, "%u ", lineNr+1);
    
    if(view->renderLineNbs){
        vec4i col = ColorFromHex(0xFD450C);//TODO: theme
        vec4i cc(col.x, col.y, col.z, 100);
        x = fonsStashMultiTextColor(font->fsContext, x, y, cc.ToUnsigned(), 
                                    linen, NULL, &previousGlyph);
    }else{
        x = fonsComputeStringAdvance(font->fsContext, " ", 1, &previousGlyph) * 0.5;
    }
    
    view->lineOffset = x;
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
                vec4i col = GetColor(&defaultTheme, token->identifier);
                if(BufferView_CursorNestIsValid(view)){
                    for(uint f = 0; f < it; f++){
                        if(f < view->startNestCount){
                            if(start[f].w){ // is valid
                                if((lineNr == start[f].x && i == start[f].y)){
                                    //TODO: pick color
                                    col = GetNestColor(&defaultTheme, 
                                                       (TokenId)start[f].z, 0);
                                    break;
                                }
                            }
                        }
                        
                        if(f < view->endNestCount){
                            if(end[f].w){ // is valid
                                if((lineNr == end[f].x && i == end[f].y)){
                                    //TODO: pick color
                                    col = GetNestColor(&defaultTheme, 
                                                       (TokenId)end[f].z, 0);
                                    break;
                                }
                            }
                        }
                    }
                    
#if 0
                    vec2ui s = view->cursor.nestStart;
                    vec2ui e = view->cursor.nestEnd;
                    if((lineNr == s.x && i == s.y) || (lineNr == e.x && i == e.y)){
                        col = vec4i(0, 255, 255, 255);
                    }
                    
#endif
                }
                
                pos += token->size;
                x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                            p, e, &previousGlyph);
            }
        }
    }
}

void _Graphics_RenderTextBlock(OpenGLFont *font, BufferView *view, Float baseHeight,
                               Transform *projection, Transform *model, vec2ui lines)
{
    Float x0 = 2.0f;
    Float x = x0;
    Float y = baseHeight;
    
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &model->m);
    
    BufferView_UpdateCursorNesting(view);
    
    for(int i = lines.x; i < lines.y; i++){
        OpenGLRenderLine(view, font, x, y, i);
        x = x0;
        y += font->fontMath.fontSizeAtRenderCall;
    }
    
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
}

void _Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                    Float lineSpan, Float baseHeight, 
                                    vec2ui cursor, vec2ui ghostcursor,
                                    vec2ui lines, Transform *model, Transform *projection)
{
    int pGlyph = -1;
    int dummyGlyph = -1;
    OpenGLFont *font = &state->font;
    Buffer *buffer = LineBuffer_GetBufferAt(bufferView->lineBuffer, cursor.x);
    
    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
    Shader_UniformInteger(font->cursorShader, "isCursor", 1);
    
    glDisable(GL_BLEND);
    
    vec4f p = OpenGLRenderCursor(state, buffer, cursor, bufferView->lineOffset, 
                                 baseHeight, lines, &pGlyph, bufferView->isActive,
                                 GetUIColorf(&defaultTheme, UICursor), 4);
    if(bufferView->isActive && ghostcursor.x >= lines.x && ghostcursor.x < lines.y){
        Float g = 0.5;
        Float a = 0.6;
        Buffer *b = LineBuffer_GetBufferAt(bufferView->lineBuffer, ghostcursor.x);
        (void) OpenGLRenderCursor(state, b, ghostcursor, bufferView->lineOffset, 
                                  baseHeight, lines, &dummyGlyph, 0, vec4f(g, g, g, a), 2);
    }
    
    if(bufferView->isActive){
        Graphics_QuadFlush(state);
        glEnable(GL_BLEND);
        Shader_UniformInteger(font->cursorShader, "isCursor", 0);
        Float g = 0.8705882; //TODO: add this to theme
        //Float g = 0.4705882;
        Graphics_QuadPush(state, vec2ui(bufferView->lineOffset, p.y), 
                          vec2ui(bufferView->lineOffset+lineSpan, p.w), 
                          vec4f(g, g, 2 * g, 0.18)
                          //vec4f(g, g, g, 0.2)
                          );
        Graphics_QuadFlush(state);
        /* Redraw whatever character we are on top of */
        if(cursor.y < buffer->count){
            int len = 0;
            uint rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, &len);
            char *chr = &buffer->data[rawp];
            if(*chr != '\t'){
                
                glEnable(GL_BLEND);
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
                
                glUseProgram(font->shader.id);
                Shader_UniformMatrix4(font->shader, "projection", &projection->m);
                Shader_UniformMatrix4(font->shader, "modelView", &model->m);
                
                //printf("Cursor at: %u, %u\n", rawp, cursor.y);
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
    OpenGLFont *font = &state->font;
    lines = BufferView_GetViewRange(bufferView);
    
    //Transform translate = state->scale * Translate(0, 18, 0);
    _Graphics_RenderTextBlock(font, bufferView, baseHeight,
                              projection, &state->scale, lines);
}

void Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                   Float lineSpan, Float baseHeight, Transform *projection)
{
    //Transform translate = state->scale * Translate(0, 18, 0);
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    vec2ui ghostcursor = BufferView_GetGhostCursorPosition(bufferView);
    vec2ui lines = BufferView_GetViewRange(bufferView);
    _Graphics_RenderCursorElements(state, bufferView, lineSpan, baseHeight, cursor, 
                                   ghostcursor, lines, &state->scale, projection);
}

void Graphics_RenderFrame(OpenGLState *state, BufferView *view,
                          Transform *projection, Theme *theme)
{
    OpenGLFont *font = &state->font;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    Geometry *geometry = &view->geometry;
    Transform *model = &state->scale;
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
    Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
    Shader_UniformInteger(font->cursorShader, "isCursor", 0);
    
    if(quad->length + 6 > quad->size){
        Graphics_QuadFlush(state);
    }
    
    vec2ui a1(u.x, u.y + font->fontMath.fontSizeAtRenderCall);
    vec2ui a0(l.x, u.y - font->fontMath.fontSizeAtRenderCall);
    
    //vec3f col = ColorRGB(theme->backgroundColor);
    //vec3f col(0.6);
    Float a = 0.18;
    Float g = 0.8705882; //TODO: add this to theme
    vec3f col = vec3f(g, g, 2 * g);
    
    if(view->descLocation == DescriptionTop){
        a0 = vec2ui(l.x, l.y);
        a1 = vec2ui(u.x, l.y + font->fontMath.fontSizeAtRenderCall);
    }else if(view->descLocation != DescriptionBottom){
        AssertA(0, "Invalid description position");
    }
    
    Graphics_QuadPush(state, a0, a1, vec4f(col.x, col.y, col.z, a));
    
    a = 0.1;
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
    Shader_UniformMatrix4(font->shader, "modelView", &model->m);
    
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
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
        Shader_UniformInteger(font->cursorShader, "isCursor", 1);
        
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
    OpenGLFont *font = &GlobalGLState.font;
    Geometry *geometry = &view->geometry;
    vec2ui bufferRes = geometry->upper - geometry->lower;
    Float width = geometry->upper.x - geometry->lower.x;
    Float scaledWidth = width * font->fontMath.invReduceScale;
    Transform projection;
    
    vec4i backgroundColor = theme->backgroundColor;
    Float fcol[] = {(Float)backgroundColor.x / 255.0f,
        (Float)backgroundColor.y / 255.0f,
        (Float)backgroundColor.z / 255.0f, 0
    };
    
    int is_animating = 0;
    OpenGLComputeProjection(geometry->lower, geometry->upper, &projection);
    
    glViewport(geometry->lower.x, geometry->lower.y,
               geometry->upper.x - geometry->lower.x, 
               geometry->upper.y - geometry->lower.y);
    
    glScissor(geometry->lower.x, geometry->lower.y,
              geometry->upper.x - geometry->lower.x, 
              geometry->upper.y - geometry->lower.y);
    
    glEnable(GL_SCISSOR_TEST);
    
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, ones);
    Float baseHeight = 0;
    if(view->descLocation == DescriptionTop){
        baseHeight = font->fontMath.fontSizeAtRenderCall;
    }
    
    SetAlpha(view->isActive ? 0 : 1);
    
    if(!BufferView_IsAnimating(view)){
        Graphics_RenderTextBlock(&GlobalGLState, view, baseHeight, &projection);
        Graphics_RenderCursorElements(&GlobalGLState, view, scaledWidth, baseHeight,
                                      &projection);
    }else{
        vec2ui range;
        vec2ui cursorAt;
        vec2ui ghostcursor;
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
            model = GlobalGLState.scale * translate;
            ghostcursor = BufferView_GetGhostCursorPosition(view);
            _Graphics_RenderTextBlock(font, view, baseHeight, &projection, &model, range);
            _Graphics_RenderCursorElements(&GlobalGLState, view, scaledWidth, baseHeight,
                                           cursorAt, ghostcursor, range, &model,
                                           &projection);
        }else{
            Graphics_RenderTextBlock(&GlobalGLState, view, baseHeight, &projection);
            Graphics_RenderCursorElements(&GlobalGLState, view, scaledWidth, baseHeight,
                                          &projection);
        }
        is_animating = 1;
    }
    
    Graphics_RenderFrame(&GlobalGLState, view, &projection, theme);
    
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
    double lastTime = GetElapsedTime();
    while(!WindowShouldCloseX11(GlobalGLState.window)){
        double currTime = GetElapsedTime();
        double dt = currTime - lastTime;
        int animating = 0;
        Float width = GlobalGLState.width;
        Float height = GlobalGLState.height;
        
        vec4i backgroundColor = defaultTheme.backgroundColor;
        Float fcol[] = {(Float)backgroundColor.x / 255.0f,
            (Float)backgroundColor.y / 255.0f,
            (Float)backgroundColor.z / 255.0f, 0
        };
        
        lastTime = currTime;
        
        glViewport(0, 0, width, height);
        glClearBufferfv(GL_COLOR, 0, fcol);
        glClearBufferfv(GL_DEPTH, 0, ones);
        
        int c = AppGetBufferViewCount();
        for(int i = 0; i < c; i++){
            BufferView *view = AppGetBufferView(i);
            animating |= Graphics_RenderBufferView(view, &defaultTheme, dt);
        }
        
        SwapBuffersX11(GlobalGLState.window);
        if(animating){
            PoolEventsX11();
        }else{
            WaitForEventsX11();
        }
        
        //glfwWaitEvents();
        usleep(10000);
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
