#include <graphics.h>
#include <types.h>
#include <thread>
#include <geometry.h>
#include <chrono>
#include <thread>
#include <app.h>
#include <vector>
//NOTE: We do have access to glad should we keep it or imlement it?
#include <glad/glad.h>
#include <sstream>
#include <keyboard.h>
#include <resources.h>
#include <view_tree.h>
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

static OpenGLState GlobalGLState;

/*
* NOTE: IMPORTANT!
* Remember that we render things in higher scale and than
* downscale with a projection, so if you want to specify 
* a rectangle or whatever in screen space [0, 0] x [width, height]
* you need to multiply the coordinates by the inverse scale:
*               font->fontMath.invReduceScale
*/


void OpenGLComputeProjection(vec2ui lower, vec2ui upper, Transform *transform){
    Float width = (Float)(upper.x - lower.x);
    Float height = (Float)(upper.y - lower.y);
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    *transform = Orthographic(0, width, height, 0, zNear, zFar);
    
}

void ActivateViewportAndProjection(OpenGLState *state, Geometry *geometry){
    uint x0 = geometry->lower.x;
    uint y0 = geometry->lower.y;
    uint w = geometry->upper.x - geometry->lower.x;
    uint h = geometry->upper.y - geometry->lower.y;
    OpenGLComputeProjection(vec2ui(x0, y0), vec2ui(x0+w,y0+h), &state->projection);
    
    glViewport(x0, y0, w, h);
    glScissor(x0, y0, w, h);
    glEnable(GL_SCISSOR_TEST);
}

void ActivateViewportAndProjection(OpenGLState *state, View *vview, GLViewport target){
    uint x0 = 0, y0 = 0, w = 0, h = 0;
    BufferView *view = View_GetBufferView(vview);
    OpenGLFont *font = &state->font;
    Geometry *geometry = &view->geometry;
    uint off = view->lineOffset * font->fontMath.reduceScale;
    switch(target){
        case ViewportLineNumbers:{
            x0 = geometry->lower.x;
            y0 = geometry->lower.y;
            w = off;
            h = geometry->upper.y - geometry->lower.y - font->fontMath.fontSizeAtDisplay;
            if(vview->descLocation == DescriptionBottom){
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
        case ViewportFrame:{
            x0 = geometry->lower.x;
            y0 = geometry->lower.y;
            w = geometry->upper.x - geometry->lower.x;
            h = geometry->upper.y - geometry->lower.y - font->fontMath.fontSizeAtDisplay;
            if(vview->descLocation == DescriptionBottom){
                y0 += font->fontMath.fontSizeAtDisplay;
            }
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

Float ScreenToGL(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    Transform inv = Inverse(state->model);
    p = inv.Point(p);
    return p.x;
}

vec2ui ScreenToGL(vec2ui u, OpenGLState * state){
    vec3f p(u.x, u.y, 0);
    Transform inv = Inverse(state->model);
    p = inv.Point(p);
    return vec2ui((uint)p.x, (uint)p.y);
}

Float GLToScreen(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    p = state->model.Point(p);
    return p.x;
}

void Graphics_SetFontSize(OpenGLState *state, Float fontSize){
    OpenGLFont *font = &state->font;
    FontMath *fMath = &font->fontMath;
    fMath->fontSizeAtRenderCall = 65; // NOTE: 65 
    fMath->fontSizeAtDisplay = fontSize; // TODO: user
    
    fMath->reduceScale = fMath->fontSizeAtDisplay / fMath->fontSizeAtRenderCall;
    fMath->invReduceScale = 1.0 / fMath->reduceScale;
    state->scale = Scale(fMath->reduceScale, fMath->reduceScale, 1);
}

int Graphics_ComputeTokenColor(char *str, Token *token, SymbolTable *symTable,
                               Theme *theme, uint lineNr, uint tid,
                               BufferView *bView, vec4i *color)
{
    if(!str || !token || !symTable || !color) return -1;
    if(token->identifier == TOKEN_ID_SPACE) return -1;
    vec4i col = GetColor(theme, token->identifier);

    if(token->identifier == TOKEN_ID_NONE){
        SymbolNode *node = SymbolTable_Search(symTable, str, token->size);
        if(node){
            col = GetColor(theme, node->id);
        }
    }else if(bView){
        if(BufferView_CursorNestIsValid(bView) && Symbol_IsTokenNest(token->identifier)){
            NestPoint *start = bView->startNest;
            NestPoint *end   = bView->endNest;
            int it = (int)Max(bView->startNestCount, bView->endNestCount);
            for(int f = 0; f < it; f++){
                if(f < bView->startNestCount){
                    if(start[f].valid){ // is valid
                        if((lineNr == start[f].position.x &&
                            tid == start[f].position.y))
                        {
                            col = GetNestColor(theme, start[f].id, start[f].depth);
                            break;
                        }
                    }
                }

                if(f < bView->endNestCount){
                    if(end[f].valid){
                        if((lineNr == end[f].position.x &&
                            tid == end[f].position.y))
                        {
                            col = GetNestColor(theme, end[f].id, end[f].depth);
                            break;
                        }
                    }
                }
            }
        }
    }

    *color = col;
    return 0;
}

vec4f Graphics_GetCursorColor(BufferView *view, Theme *theme, int ghost){
    if(!theme->dynamicCursor){
        return GetUIColorf(theme, UICursor);
    }else{
        vec4i color;
        vec2ui cursor;
        int r = -1;
        Tokenizer *tokenizer = view->tokenizer;
        SymbolTable *symTable = tokenizer->symbolTable;

        if(ghost){
            cursor = BufferView_GetGhostCursorPosition(view);
        }else{
            cursor = BufferView_GetCursorPosition(view);
        }

        Buffer *buffer = BufferView_GetBufferAt(view, cursor.x);

        uint tid = Buffer_GetTokenAt(buffer, cursor.y);
        if(tid >= buffer->tokenCount){
            return GetUIColorf(theme, UICursor);
        }

        Token *token = &buffer->tokens[tid];
        if(token == nullptr){
            return GetUIColorf(theme, UICursor);
        }

        if(!Symbol_IsTokenQueriable(token->identifier)){
            return GetUIColorf(theme, UICursor);
        }

        char *str = &buffer->data[token->position];

        r = Graphics_ComputeTokenColor(str, token, symTable, theme, cursor.x,
                                       token->identifier, nullptr, &color);
        if(r == -1){
            return GetUIColorf(theme, UICursor);
        }

        return ColorFromInt(color);
    }
}

void Graphics_PrepareTextRendering(OpenGLState *state, Transform *projection,
                                   Transform *model)
{
    OpenGLFont *font = &state->font;
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &model->m);
}

void Graphics_PushText(OpenGLState *state, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph)
{
    OpenGLFont *font = &state->font;
    x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                text, text+len, pGlyph);
}

void Graphics_FlushText(OpenGLState *state){
    OpenGLFont *font = &state->font;
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

vec2ui Graphics_GetMouse(OpenGLState *state){
    return state->mouse;
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

static void _OpenGLUpdateProjections(OpenGLState *state, int width, int height){
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
void Timing_Update(){
    lastTime = GetElapsedTime();
}

void WindowOnSizeChange(int width, int height){
    _OpenGLUpdateProjections(&GlobalGLState, width, height);
}

void WinOnMouseMotion(int x, int y){
    //printf("Mouse to %d %d\n", x, y);
    GlobalGLState.mouse = vec2ui((uint)x, (uint)y);
}

void WindowOnScroll(int is_up){ 
    int scrollRange = 5;
    int x = 0, y = 0;
    //TODO: When view is not active this is triggering cursor jump
    //      when going up, debug it!
    GetLastRecordedMousePositionX11(GlobalGLState.window, &x, &y);
    
    View *view = AppGetViewAt(x, GlobalGLState.height - y);
    if(view){
        ViewState state = View_GetState(view);
        if(state == View_FreeTyping){
            BufferView *bView = View_GetBufferView(view);
            NullRet(bView);
            NullRet(bView->lineBuffer);
            BufferView_StartScrollViewTransition(bView, is_up ? -scrollRange : scrollRange,
                                                 kTransitionScrollInterval);
            Timing_Update();
        }
    }
}

void WindowOnClick(int x, int y){
    vec2ui mouse = AppActivateViewAt(x, GlobalGLState.height - y);
    
    //TODO: States
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    if(state == View_FreeTyping){
        BufferView *bufferView = View_GetBufferView(view);
        uint dy = view->geometry.upper.y - view->geometry.lower.y;
        int lineNo = BufferView_ComputeTextLine(bufferView, dy - mouse.y, view->descLocation);
        if(lineNo < 0) return;
        
        lineNo = Clamp((uint)lineNo, (uint)0, BufferView_GetLineCount(bufferView)-1);
        
        uint colNo = 0;
        Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
        x = ScreenToGL(mouse.x, &GlobalGLState) - bufferView->lineOffset;
        if(x > 0){
            // TODO: It seems when we have a tab on a line and we click to select
            // a character it seems we are jumping exactly tabSpacing - 1 ahead
            colNo = fonsComputeStringOffsetCount(GlobalGLState.font.fsContext,
                                                 buffer->data, x);
            BufferView_CursorToPosition(bufferView, (uint)lineNo, colNo);
            BufferView_GhostCursorFollow(bufferView);
        }
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
    RegisterOnMouseMotionCallback(window, WinOnMouseMotion);
    RegisterOnSizeChangeCallback(window, WindowOnSizeChange);
    RegisterOnFocusChangeCallback(window, WinOnFocusChange);
}

void OpenGLFontSetup(OpenGLState *state){
    uint filesize = 0;
    char *fontfileContents = nullptr;
    OpenGLFont *font = &state->font;
    
    //TODO: These should not be files
    const char *v0 = "/home/felipe/Documents/Mayhem/shaders/text.v.glsl";
    const char *f0 = "/home/felipe/Documents/Mayhem/shaders/text.f.glsl";
    const char *v1 = "/home/felipe/Documents/Mayhem/shaders/cursor.v.glsl";
    const char *f1 = "/home/felipe/Documents/Mayhem/shaders/cursor.f.glsl";
    
    //const char *ttf = "/home/felipe/Documents/Mayhem/fonts/liberation-mono.ttf";
    //const char *ttf = "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf";
    uint vertex    = Shader_CompileFile(v0, SHADER_TYPE_VERTEX);
    uint fragment  = Shader_CompileFile(f0, SHADER_TYPE_FRAGMENT);
    uint cvertex   = Shader_CompileFile(v1, SHADER_TYPE_VERTEX);
    uint cfragment = Shader_CompileFile(f1, SHADER_TYPE_FRAGMENT);
    
    Shader_Create(font->shader, vertex, fragment);
    Shader_Create(font->cursorShader, cvertex, cfragment);
    
    font->sdfSettings.sdfEnabled = 0;
    font->fsContext = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
    
    //fontfileContents = GetFileContents(ttf, &filesize);
    fontfileContents = (char *)FONT_liberation_mono_ttf;
    filesize = FONT_liberation_mono_ttf_len;
    
    font->fontId = fonsAddFontSdfMem(font->fsContext, "Default", 
                                     (uint8 *)fontfileContents, filesize, 0,
                                     font->sdfSettings);
    AssertA(font->fontId != FONS_INVALID, "Failed to create font");
    Graphics_SetFontSize(state, 19); // TODO: User
}

void Graphics_QuadPushBorder(OpenGLState *state, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col)
{
    Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(w, y1), col);
    Graphics_QuadPush(state, vec2ui(x0, y1-w), vec2ui(x1, y1), col);
    Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y0+w), col);
    Graphics_QuadPush(state, vec2ui(x1-w, y0), vec2ui(x1, y1), col);
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

void Graphics_QuadFlush(OpenGLState *state, int blend){
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

void Graphics_LineFlush(OpenGLState *state, int blend){
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

/*
* TODO: It is interesting to attempt to render 1 +2pages and allow
*       for a translation matrix. It might allow us to better represent
*       transitions and the viewing interface? Also it allows us to by-pass
*       the 'translate at end of file' issue we have right now.
*/
void OpenGLEntry(){
    BufferView *bufferView = AppGetActiveBufferView();
    OpenGLState *state = &GlobalGLState;
    OpenGLFont *font = &state->font;
    
    _OpenGLStateInitialize(state);
    OpenGLInitialize(state);
    OpenGLFontSetup(state);
    
    _OpenGLUpdateProjections(state, state->width, state->height);
    
    BufferView_CursorTo(bufferView, 0);
    Timing_Update();

    while(!WindowShouldCloseX11(state->window)){
        double currTime = GetElapsedTime();
        double dt = currTime - lastTime;
        int animating = 0;
        
        lastTime = currTime;
        AppUpdateViews();

        //TODO: Move this
        glUseProgram(font->shader.id);
        Shader_UniformInteger(font->shader, "enable_contrast",
                              ThemeNeedsEffect(defaultTheme));

        ViewTreeIterator iterator;
        ViewTree_Begin(&iterator);
        while(iterator.value){
            View *view = iterator.value->view;
            RenderList *pipeline = View_GetRenderPipeline(view);
            BufferView_UpdateCursorNesting(View_GetBufferView(view));

            // Sets the alpha for the current view rendering stages, makes the dimm effect
            SetAlpha(view->isActive ? 0 : 1);

            for(uint s = 0; s < pipeline->stageCount; s++){
                state->model = state->scale; // reset model
                animating |= pipeline->stages[s].renderer(view, state, defaultTheme, dt);
            }

            ViewTree_Next(&iterator);
        }

        SwapBuffersX11(state->window);
        if(animating){
            PoolEventsX11();
        }else{
            WaitForEventsX11();
        }
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    DestroyWindowX11(state->window);
    TerminateX11();
    
    state->running = 0;
    state->window = nullptr;
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
