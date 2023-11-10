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
#include <image_util.h>
#include <file_provider.h>
#include <parallel.h>
#include <dbgapp.h>
#include <widgets.h>
#include <dbgwidget.h>
#include <popupwindow.h>
#include <modal.h>

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
* This is because of the cheap AA and better scaling for larger fonts
*/

#define TOOGLE_VAR(var) (var) = !(var)


std::string translateGLError(int errorCode){
    std::string error;
    switch (errorCode){
        case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        default: error = "Unknown Error";
    }
    return error;
}

void __gl_validate(const char *cmd, int line, const char *fileName){
    int val = glGetError();
    if(val != GL_NO_ERROR){
        std::string msg = translateGLError(val);
        std::cout << "(" << fileName << " : " << line << ") " << cmd << " => " << msg.c_str()
                   << "[" << val << "]" << std::endl;
        Assert(false);
        //exit(0);
    }
}

#if defined(MEMORY_DEBUG)

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049

void __gpu_gl_get_memory_usage(){
    int total_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &total_mem_kb);

    int cur_avail_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb);
    total_mem_kb /= 1024;
    cur_avail_mem_kb /= 1024;
    printf("[GPU] Current Available: %d / %d (MB)\n", cur_avail_mem_kb, total_mem_kb);
}
#endif

void __gl_clear_errors(){
    while(glGetError()){
        ;
    }
}

void OpenGLClearErrors(){
    while(glGetError()) ;
}

void *Graphics_GetBaseWindow(){
    return (void *)GlobalGLState.window;
}

WidgetWindow *Graphics_GetBaseWidgetWindow(){
    return GlobalGLState.gWidgets.wwindow;
}

DbgWidgetButtons *Graphics_GetDbgWidget(){
    return GlobalGLState.gWidgets.wdbgBt;
}

DbgWidgetExpressionViewer *Graphics_GetDbgExpressionViewer(){
    return GlobalGLState.gWidgets.wdbgVw;
}

OpenGLState *Graphics_GetGlobalContext(){
    return &GlobalGLState;
}

void Graphics_ToogleCursorSegment(){
    TOOGLE_VAR(GlobalGLState.params.cursorSegments);
}

void OpenGLFontCopy(OpenGLFont *dst, OpenGLFont *src){
    dst->fsContext = src->fsContext;
    dst->sdfSettings = src->sdfSettings;
    dst->shader = src->shader;
    dst->cursorShader = src->cursorShader;
    dst->fontId = src->fontId;
    dst->fontMath = src->fontMath;
}

uint Graphics_GetFontSize(){
    return GlobalGLState.font.fontMath.fontSizeAtDisplay;
}

static void ResetGLCursor(OpenGLCursor *cursor){
    cursor->pMin = vec2f(0,0);
    cursor->pMax = vec2f(0,0);
    cursor->valid = 0;
    cursor->pGlyph = -1;
    cursor->textPos = vec2f(0,0);
    cursor->currentDif = 0;
}

void OpenGLResetCursors(OpenGLState *state){
    ResetGLCursor(&state->glCursor);
    ResetGLCursor(&state->glGhostCursor);
}

std::string OpenGLValidateErrorStr(int *err){
    int val = glGetError();
    std::string error;
    if(val != GL_NO_ERROR){
        switch(val){
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: error = "Unknown Error";
        }
    }
    if(err) *err = val;
    return error;
}

void OpenGLValidateErrors(const char *fn, int line, const char *file){
    int val = 0;
    std::string error = OpenGLValidateErrorStr(&val);
    if(error.size() > 0){
        printf("OpenGL Error: %s (%d) -- %s (%s:%d)\n", error.c_str(),
               val, fn, file, line);
        AssertA(val == GL_NO_ERROR, "Invalid OpenGL call");
    }
}

void OpenGLComputeProjection(vec2ui lower, vec2ui upper, Transform *transform){
    Float width = (Float)(upper.x - lower.x);
    Float height = (Float)(upper.y - lower.y);
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    *transform = Orthographic(0, width, height, 0, zNear, zFar);
}

void ActivateViewportAndProjection(OpenGLState *state, Geometry *geometry,
                                   bool with_scissors)
{
    uint x0 = geometry->lower.x;
    uint y0 = geometry->lower.y;
    uint w = geometry->upper.x - geometry->lower.x;
    uint h = geometry->upper.y - geometry->lower.y;
    OpenGLComputeProjection(vec2ui(x0, y0), vec2ui(x0+w,y0+h), &state->projection);

    glViewport(x0, y0, w, h);
    if(with_scissors){
        glScissor(x0, y0, w, h);
        glEnable(GL_SCISSOR_TEST);
    }
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

Float ScreenToTransform(Float x, Transform model){
    vec3f p(x, 0, 0);
    Transform inv = Inverse(model);
    p = inv.Point(p);
    return p.x;
}

Float ScreenToGL(Float x, OpenGLState *state){
    return ScreenToTransform(x, state->model);
}

vec2ui ScreenToGL(vec2ui u, OpenGLState * state){
    vec3f p(u.x, u.y, 0);
    Transform inv = Inverse(state->model);
    p = inv.Point(p);
    return vec2ui((uint)p.x, (uint)p.y);
}

vec2f ScreenToGLf(vec2f u, OpenGLState * state){
    vec3f p(u.x, u.y, 0);
    Transform inv = Inverse(state->scale);
    p = inv.Point(p);
    return vec2f(p.x, p.y);
}

vec2f GLToScreen(vec2f u, OpenGLState *state){
    vec3f p(u.x, u.y, 0);
    p = state->model.Point(p);
    return vec2f(p.x, p.y);
}

Float GLToScreen(Float x, OpenGLState *state){
    vec3f p(x, 0, 0);
    p = state->model.Point(p);
    return p.x;
}

vec2f Graphics_ComputeCenteringStart(OpenGLFont *font, const char *text,
                                     uint len, Geometry *geometry, bool in_place)
{
    int pGlyph = -1;
    Float x0 = geometry->lower.x * font->fontMath.invReduceScale;
    Float y0 = geometry->lower.y * font->fontMath.invReduceScale;
    Float y = (geometry->upper.y - geometry->lower.y) * font->fontMath.invReduceScale * 0.5;
    Float x = (geometry->upper.x - geometry->lower.x) * font->fontMath.invReduceScale * 0.5;
    Float dx = fonsComputeStringAdvance(font->fsContext, (char *)text, len, &pGlyph) * 0.5;

    x -= dx;
    y -= font->fontMath.fontSizeAtRenderCall * 0.5;

    vec2f xy(x, y);
    if(!in_place){
        xy += vec2f(x0, y0);
    }
    return Max(vec2f(0,0), xy);
}

Float Graphics_GetTokenXSize(OpenGLState *state, Buffer *buffer, uint at,
                             int &previousGlyph)
{
    OpenGLFont *font = &state->font;
    if(buffer->taken == 0 || buffer->tokenCount <= at) return 0;
    Token *token = &buffer->tokens[at];
    char *p = &buffer->data[token->position];
    return fonsComputeStringAdvance(font->fsContext, p, token->size, &previousGlyph);
}

Float Graphics_GetTokenXPos(OpenGLState *state, Buffer *buffer,
                            uint upto, int &previousGlyph)
{
    Float x = 0;
    OpenGLFont *font = &state->font;
    uint pos = 0;

    if(buffer->taken == 0 || buffer->tokenCount <= upto) return x;

    for(uint i = 0; i < upto; i++){
        Token *token = &buffer->tokens[i];
        while((int)pos < token->position){
            char v = buffer->data[pos];
            if(v == '\r' || v == '\n'){
                pos++;
                continue;
            }

            if(v == ' ' || v == '\t'){
                x += fonsComputeStringAdvance(font->fsContext, (char *)&v, 1,
                                              &previousGlyph);
                pos ++;
                continue;
            }

            if(v == 0){
                return x;
            }

            BUG();
            return x;
        }

        if(token->identifier == TOKEN_ID_SPACE){
            // nothing
        }else if(token->position < (int)buffer->taken){
            char *p = &buffer->data[token->position];
            char *e = &buffer->data[token->position + token->size];

            pos += token->size;
            x += fonsComputeStringAdvance(font->fsContext, p, e-p, &previousGlyph);
        }
    }

    return x;
}

vec2f Graphics_GetLineYPos(OpenGLState *state, vec2ui visible, uint i, View *vview){
    OpenGLFont *font = &state->font;
    Float y0 = ((Float)i - (Float)visible.x) * font->fontMath.fontSizeAtRenderCall;
    if(vview->descLocation == DescriptionTop){
        y0 += font->fontMath.fontSizeAtRenderCall;
    }

    return vec2f(y0, y0 + font->fontMath.fontSizeAtRenderCall);
}

void Graphics_ComputeTransformsForFontSize(OpenGLFont *font, Float fontSize,
                                           Transform *model, Float reference)
{
    FontMath *fMath = &font->fontMath;
    fMath->fontSizeAtRenderCall = reference;
    fMath->fontSizeAtDisplay = fontSize;

    fMath->reduceScale = fMath->fontSizeAtDisplay / fMath->fontSizeAtRenderCall;
    fMath->invReduceScale = 1.0 / fMath->reduceScale;
    *model = Scale(fMath->reduceScale, fMath->reduceScale, 1);
}

uint Graphics_GetDefaultLineHeight(){
    return GlobalGLState.font.fontMath.fontSizeAtDisplay;
}

void Graphics_SetFontSize(OpenGLState *state, Float fontSize, Float reference){
    OpenGLFont *font = &state->font;
    Graphics_ComputeTransformsForFontSize(font, fontSize, &state->scale, reference);
}

void Graphics_SetDefaultFontSize(uint fontSize){
    Graphics_SetFontSize(&GlobalGLState, fontSize);
}

int Graphics_ComputeTokenColor(char *str, Token *token, SymbolTable *symTable,
                               Theme *theme, uint lineNr, uint tid,
                               BufferView *bView, vec4i *color)
{
    if(!str || !token || !symTable || !color) return -1;
    if(token->identifier == TOKEN_ID_SPACE) return -1;
    vec4i col = GetColor(theme, token->identifier);
    /* handle explicit overriden values, i.e.: functions and none */
    if(Symbol_IsTokenOverriden(token->identifier)){
        SymbolNode *node = SymbolTable_Search(symTable, str, token->size);
        /* explicit search for user types for better rendering, better view */
        SymbolNode *res = node;
        while(res != nullptr){
            res = SymbolTable_SymNodeNext(res, str, token->size);
            if(res){
                if(!(Symbol_IsTokenOverriden(res->id))){
                    node = res;
                    break;
                }
            }
        }
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

    if(bView){
        LineBuffer *lineBuffer = BufferView_GetLineBuffer(bView);
        if(LineBuffer_IsInsideCopySection(lineBuffer, tid, lineNr)){
            // this token is being pasted interpolate the color
            CopySection section;
            vec4i tp = GetUIColor(theme, UIPasteColor); // theme paste color

            LineBuffer_GetCopySection(lineBuffer, &section);
            Float f = section.currTime / section.interval;
            Float alpha = Lerp(0.f, 255.f, f);
            vec3f interp = Lerp(vec3f((Float)tp.x,(Float)tp.y,(Float)tp.z),
                                vec3f((Float)col.x,(Float)col.y,(Float)col.z), f);

            col = vec4i((uint)interp.x, (uint)interp.y, (uint)interp.z, (uint)alpha);

            // hacky way to inform the main rendering loop that even if this view
            // is not jumping it is transitioning by chromatic interpolation
            bView->is_transitioning = 1;
        }
    }

    *color = col;
    return 0;
}

vec4f Graphics_GetCursorColor(BufferView *view, Theme *theme, int ghost){
    // account for dual mode
    vec4f col;
    UIElement element = UICursor;
    int dualMode = DualModeGetState();

    if(!theme->dynamicCursor){
        col = GetUIColorf(theme, element);
    }else{
        vec4i color;
        vec2ui cursor;
        int r = -1;
        Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(view->lineBuffer);
        SymbolTable *symTable = tokenizer->symbolTable;

        if(ghost){
            cursor = BufferView_GetGhostCursorPosition(view);
        }else{
            cursor = BufferView_GetCursorPosition(view);
        }

        Buffer *buffer = BufferView_GetBufferAt(view, cursor.x);

        uint tid = Buffer_GetTokenAt(buffer, cursor.y);
        if(tid >= buffer->tokenCount){
            return GetUIColorf(theme, element);
        }

        Token *token = &buffer->tokens[tid];
        if(token == nullptr){
            return GetUIColorf(theme, element);
        }

        if(!Symbol_IsTokenQueriable(token->identifier)){
            return GetUIColorf(theme, element);
        }

        char *str = &buffer->data[token->position];

        r = Graphics_ComputeTokenColor(str, token, symTable, theme, cursor.x,
                                       token->identifier, nullptr, &color);
        if(r == -1)
            col = GetUIColorf(theme, element);
        else
            col = ColorFromInt(color);
    }

    if(dualMode == ENTRY_MODE_LOCK){
        // TODO: do we need a theme option for this?
        Float w = col.w;
        col *= 0.7;
        col.w = w;
    }
    return col;
}

void Graphics_PrepareTextRendering(OpenGLFont *font, Transform *projection,
                                   Transform *model)
{
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &model->m);
}

void Graphics_PushText(OpenGLFont *font, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph)
{
    x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                text, text+len, pGlyph);
}

void Graphics_FlushText(OpenGLFont *font){
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void Graphics_PrepareTextRendering(OpenGLState *state, Transform *projection,
                                   Transform *model)
{
    OpenGLFont *font = &state->font;
    Graphics_PrepareTextRendering(font, projection, model);
}

void Graphics_PushText(OpenGLState *state, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph)
{
    OpenGLFont *font = &state->font;
    Graphics_PushText(font, x, y, text, len, col, pGlyph);
}

void Graphics_FlushText(OpenGLState *state){
    OpenGLFont *font = &state->font;
    Graphics_FlushText(font);
}

vec2ui Graphics_GetMouse(OpenGLState *state){
    return state->mouse.position;
}

static void OpenGLTextureInitialize(OpenGLState *state){
    state->texBinds = 0;
    state->glQuadImageBuffer.units = 0;
}

void OpenGLBufferInitialize(OpenGLImageQuadBuffer *buffer, int n){
    buffer->vertex = AllocatorGetN(Float, 3 * n);
    buffer->tex = AllocatorGetN(Float, 2 * n);
    buffer->size = n;
    buffer->length = 0;
    buffer->units = 0;
    for(int i = 0; i < MAX_TEXTURES_PER_BATCH; i++){
        buffer->textureIds[i] = 0;
    }

    glGenVertexArrays(1, &buffer->vertexArray);
    glBindVertexArray(buffer->vertexArray);

    glGenBuffers(1, &buffer->vertexBuffer);
    glGenBuffers(1, &buffer->texBuffer);
}

void OpenGLBufferContextDelete(OpenGLBuffer *buffer){
    glDeleteVertexArrays(1, &buffer->vertexArray);
}

void OpenGLBufferContextDelete(OpenGLImageQuadBuffer *buffer){
    glDeleteVertexArrays(1, &buffer->vertexArray);
}

void OpenGLImageBufferInitializeFrom(OpenGLImageQuadBuffer *buffer,
                                     OpenGLImageQuadBuffer *other)
{
    buffer->vertex = other->vertex;
    buffer->tex = other->tex;
    buffer->size = other->size;
    buffer->length = other->length;
    buffer->vertexBuffer = other->vertexBuffer;
    buffer->texBuffer = other->texBuffer;
    buffer->units = 0;
    for(int i = 0; i < MAX_TEXTURES_PER_BATCH; i++){
        buffer->textureIds[i] = 0;
    }
    glGenVertexArrays(1, &buffer->vertexArray);
}

void OpenGLBufferInitializeFrom(OpenGLBuffer *buffer, OpenGLBuffer *other){
    buffer->vertex = other->vertex;
    buffer->colors = other->colors;
    buffer->size = other->size;
    buffer->length = other->length;
    buffer->vertexBuffer = other->vertexBuffer;
    buffer->colorBuffer = other->colorBuffer;

    glGenVertexArrays(1, &buffer->vertexArray);
}

void OpenGLBufferInitialize(OpenGLBuffer *buffer, int n){
    buffer->vertex = AllocatorGetN(Float, 2 * n);
    buffer->colors = AllocatorGetN(Float, 4 * n);
    buffer->size = n;
    buffer->length = 0;

    glGenVertexArrays(1, &buffer->vertexArray);
    glBindVertexArray(buffer->vertexArray);

    glGenBuffers(1, &buffer->vertexBuffer);
    glGenBuffers(1, &buffer->colorBuffer);
}

static void _OpenGLBufferPushVertex(OpenGLImageQuadBuffer *buffer, Float x, Float y,
                                    vec2f tex, uint mid)
{
    int addedUnit = 1;
    int texId = -1;
    for(uint i = 0; i < buffer->units; i++){
        if(buffer->textureIds[i] == mid){
            addedUnit = 0;
            texId = i;
            break;
        }
    }

    if(addedUnit){
        buffer->textureIds[buffer->units] = mid;
        texId = buffer->units;
        buffer->units++;
    }

    buffer->vertex[3 * buffer->length + 0] = x;
    buffer->vertex[3 * buffer->length + 1] = y;
    buffer->vertex[3 * buffer->length + 2] = (Float)texId;

    buffer->tex[2 * buffer->length + 0] = tex.x;
    buffer->tex[2 * buffer->length + 1] = tex.y;

    buffer->length ++;
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

static void _OpenGLInitGlobalWidgets(OpenGLState *state){
    GlobalWidgets *gw = &state->gWidgets;
    gw->wwindow = new WidgetWindow(state->window, AppGetFreetypingBinding(),
                                   vec2f(0), vec2f(state->width, state->height));
    gw->wdbgBt = new DbgWidgetButtons;
    gw->wdbgVw = new DbgWidgetExpressionViewer;
}

static void _OpenGLStateInitialize(OpenGLState *state){
    int targetWidth = 1600;
    int targetHeight = 900;
    state->eventInterval = Infinity;
    state->font.fsContext = nullptr;
    state->font.fontId = -1;
    state->window = nullptr;
    state->width = targetWidth;
    state->height = targetHeight;
    state->cursorVisible = true;
    state->cursorBlinkLastTime = 0;
    state->maxSamplingRate = 1.f / 120.f; // based on 120 fps
    memset(&state->font.sdfSettings, 0x00, sizeof(FONSsdfSettings));
    state->params.cursorSegments = true;
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

void WindowOnSizeChange(int width, int height, void *){
    _OpenGLUpdateProjections(&GlobalGLState, width, height);
}

static bool MouseEventFilter(int x, int y){
    OpenGLState *state = &GlobalGLState;
    WidgetWindow *ww = state->gWidgets.wwindow;
    if(ww->WidgetCount() == 0) return false;
    vec2f m(x, state->height - y);
    for(Widget *widget : ww->widgetList){
        Geometry geometry = widget->GetGeometry();
        if(Geometry_IsPointInside(&geometry, m)){
            return true;
        }
    }
    return false;
}

// Mouse motion generates a lot of cpu usage...
void WinOnMouseMotion(int x, int y, void *){
    GlobalGLState.mouse.position = vec2ui((uint)x, (uint)y);
    AppHandleMouseMotion(x, y, &GlobalGLState);
}

void WindowOnMouseClick(int x, int y, void *){
    if(!MouseEventFilter(x, y)){
        AppHandleMouseClick(x, y, &GlobalGLState);
    }
}

void WindowOnScroll(int is_up, void *){
    int x = 0, y = 0;
    //TODO: When view is not active this is triggering cursor jump
    //      when going up, debug it!
    GetLastRecordedMousePositionX11(GlobalGLState.window, &x, &y);
    if(!MouseEventFilter(x, y)){
        AppHandleMouseScroll(x, y, is_up, &GlobalGLState);
    }
}

void WindowOnPress(int x, int y, void *){
    if(!MouseEventFilter(x, y)){
        GlobalGLState.mouse.isPressed = true;
        AppHandleMousePress(x, y, &GlobalGLState);
    }
}

void WinOnFocusChange(bool in, long unsigned int id, void *){
    KeyboardResetState();
}

void WindowOnDoubleClick(int x, int y, void *){
    if(!MouseEventFilter(x, y)){
        AppHandleDoubleClick(x, y, &GlobalGLState);
    }
}

int Font_SupportsCodepoint(int codepoint){
    int g = fonsGetGlyphIndex(GlobalGLState.font.fsContext, codepoint);
    return g != 0;
}

void RegisterInputs(WindowX11 *window){
    RegisterOnScrollCallback(window, WindowOnScroll, nullptr);
    RegisterOnMouseLeftClickCallback(window, WindowOnMouseClick, nullptr);
    RegisterOnMousePressedCallback(window, WindowOnPress, nullptr);
    RegisterOnMouseDoubleClickCallback(window, WindowOnDoubleClick, nullptr);
    RegisterOnMouseMotionCallback(window, WinOnMouseMotion, nullptr);
    RegisterOnSizeChangeCallback(window, WindowOnSizeChange, nullptr);
    RegisterOnFocusChangeCallback(window, WinOnFocusChange, nullptr);
}

void OpenGLFontSetup(OpenGLState *state){
    uint filesize = 0;
    char *fontfileContents = nullptr;
    OpenGLFont *font = &state->font;

    uint vertex    = Shader_CompileSource(shader_text_v, SHADER_TYPE_VERTEX);
    uint fragment  = Shader_CompileSource(shader_text_f, SHADER_TYPE_FRAGMENT);
    uint cvertex   = Shader_CompileSource(shader_cursor_v, SHADER_TYPE_VERTEX);
    uint cfragment = Shader_CompileSource(shader_cursor_f, SHADER_TYPE_FRAGMENT);
    uint ivertex   = Shader_CompileSource(shader_icon_v, SHADER_TYPE_VERTEX);
    uint ifragment = Shader_CompileSource(shader_icon_f, SHADER_TYPE_FRAGMENT);
    uint bvertex   = Shader_CompileSource(shader_button_v, SHADER_TYPE_VERTEX);
    uint bfragment = Shader_CompileSource(shader_button_f, SHADER_TYPE_FRAGMENT);

    Shader_Create(font->shader, vertex, fragment);
    Shader_Create(font->cursorShader, cvertex, cfragment);
    Shader_Create(state->imageShader, ivertex, ifragment);
    Shader_Create(state->buttonShader, bvertex, bfragment);

    font->sdfSettings.sdfEnabled = 0;
    font->fsContext = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);

    //fontfileContents = GetFileContents(ttf, &filesize);
    fontfileContents = (char *)FONT_liberation_mono_ttf;
    filesize = FONT_liberation_mono_ttf_len;

    font->fontId = fonsAddFontSdfMem(font->fsContext, "Default",
                                     (uint8 *)fontfileContents, filesize, 0,
                                     font->sdfSettings);
    AssertA(font->fontId != FONS_INVALID, "Failed to create font");
    Graphics_SetFontSize(state, AppGetFontSize());
}

void Graphics_QuadPushBorder(OpenGLBuffer *quadB, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col)
{
    Graphics_QuadPush(quadB, vec2ui(x0, y0), vec2ui(x0+w, y1), col); // left
    Graphics_QuadPush(quadB, vec2ui(x0, y1-w), vec2ui(x1, y1), col); // bottom
    Graphics_QuadPush(quadB, vec2ui(x0, y0), vec2ui(x1, y0+w), col); // top
    Graphics_QuadPush(quadB, vec2ui(x1-w, y0), vec2ui(x1, y1), col); // right
}

void Graphics_QuadPushBorder(OpenGLState *state, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col)
{
    OpenGLBuffer *quad = &state->glQuadBuffer;
    Graphics_QuadPushBorder(quad, x0, y0, x1, y1, w, col);
}

static void OpenGLLoadIcons(OpenGLState *state){
    Graphics_TextureInit(state, folder_png, folder_png_len, ".folder", FILE_EXTENSION_FOLDER);
    Graphics_TextureInit(state, cmake_png, cmake_png_len, ".cmake", FILE_EXTENSION_CMAKE);
    Graphics_TextureInit(state, cpp_png, cpp_png_len, ".cpp", FILE_EXTENSION_CPP);
    Graphics_TextureInit(state, cppheader_png, cppheader_png_len, ".cppheader");
    Graphics_TextureInit(state, py_png, py_png_len, ".py", FILE_EXTENSION_PYTHON);
    Graphics_TextureInit(state, glsl_png, glsl_png_len, ".glsl", FILE_EXTENSION_GLSL);
    Graphics_TextureInit(state, cuda_png, cuda_png_len, ".cu", FILE_EXTENSION_CUDA);
    Graphics_TextureInit(state, text_png, text_png_len, ".txt", FILE_EXTENSION_TEXT);
    Graphics_TextureInit(state, font_png, font_png_len, ".ttf", FILE_EXTENSION_FONT);
    Graphics_TextureInit(state, wave_dark_png, wave_dark_png_len,
                         ".lit_dark", FILE_EXTENSION_LIT_DARK);
    Graphics_TextureInit(state, wave_white_png, wave_white_png_len,
                         ".lit_white", FILE_EXTENSION_LIT_WHITE);
    Graphics_TextureInit(state, wave_white_png, wave_white_png_len,
                         ".lit", FILE_EXTENSION_LIT);
}

void OpenGLInitialize(OpenGLState *state){
    //DEBUG_MSG("Initializing OpenGL graphics\n");
    OpenGLBuffer *quad      = &state->glQuadBuffer;
    OpenGLBuffer *lines     = &state->glLineBuffer;
    OpenGLImageQuadBuffer *imageQuad = &state->glQuadImageBuffer;
    int width = state->width;
    int height = state->height;

    InitializeX11();
    SetSamplesX11(16);
    SetOpenGLVersionX11(3, 3);
    state->mouse.position = vec2ui(0);
    state->mouse.isPressed = false;
    state->window = CreateWindowX11(width, height, "Cody - 0.0.1");

    AssertErr(gladLoadGL() != 0, "Failed to load OpenGL pointers");
    GLenum error = glGetError();
    AssertErr(error == GL_NO_ERROR, "Failed to setup opengl context");

    OpenGLBufferInitialize(quad, 1024);
    OpenGLBufferInitialize(imageQuad, 1024);
    OpenGLBufferInitialize(lines, 1024);
    OpenGLTextureInitialize(state);
    AppInitialize();

    SwapIntervalX11(state->window, 0);
    RegisterInputs(state->window);
    OpenGLLoadIcons(state);
}

int Graphics_ImagePush(OpenGLImageQuadBuffer *quad, vec2ui left, vec2ui right, int mid){
    Float x0 = left.x, y0 = left.y, x1 = right.x, y1 = right.y;
    if(!(quad->units+1 < MAX_TEXTURES_PER_BATCH)){
        return 1;
    }

    _OpenGLBufferPushVertex(quad, x0, y0, vec2f(0, 0), mid);
    _OpenGLBufferPushVertex(quad, x1, y1, vec2f(1, 1), mid);
    _OpenGLBufferPushVertex(quad, x1, y0, vec2f(1, 0), mid);

    _OpenGLBufferPushVertex(quad, x0, y0, vec2f(0, 0), mid);
    _OpenGLBufferPushVertex(quad, x0, y1, vec2f(0, 1), mid);
    _OpenGLBufferPushVertex(quad, x1, y1, vec2f(1, 1), mid);
    return 0;
}

int Graphics_ImagePush(OpenGLState *state, vec2ui left, vec2ui right, int mid){
    OpenGLImageQuadBuffer *quad = &state->glQuadImageBuffer;
    return Graphics_ImagePush(quad, left, right, mid);
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

void Graphics_QuadPush(OpenGLBuffer *quad, vec2ui left, vec2ui right, vec4f color){
    Float x0 = left.x, y0 = left.y, x1 = right.x, y1 = right.y;
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

void Graphics_ImageFlush(OpenGLState *state, OpenGLImageQuadBuffer *oquad){
    OpenGLImageQuadBuffer *quad = &state->glQuadImageBuffer;
    if(oquad){
        quad = oquad;
    }

    if(quad->length > 0){
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(state->imageShader.id);
        Shader_UniformMatrix4(state->imageShader, "projection", &state->projection.m);
        Shader_UniformMatrix4(state->imageShader, "modelView", &state->scale.m);
        OpenGLCHK(glBindVertexArray(quad->vertexArray));
        OpenGLCHK(glEnableVertexAttribArray(0));
        OpenGLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad->vertexBuffer));
        OpenGLCHK(glBufferData(GL_ARRAY_BUFFER, quad->length * 3 * sizeof(Float),
                               quad->vertex, GL_DYNAMIC_DRAW));
        OpenGLCHK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL));

        glEnableVertexAttribArray(1);
        OpenGLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad->texBuffer));
        OpenGLCHK(glBufferData(GL_ARRAY_BUFFER, quad->length * 2 * sizeof(Float),
                               quad->tex, GL_DYNAMIC_DRAW));
        OpenGLCHK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL));


        Graphics_BindImages(state, quad);
        OpenGLCHK(glDrawArrays(GL_TRIANGLES, 0, quad->length));

        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    quad->length = 0;
    quad->units = 0;
}

void Graphics_QuadFlush(OpenGLBuffer *quad, int blend){
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

void Graphics_QuadFlush(OpenGLState *state, int blend){
    OpenGLBuffer *quad = &state->glQuadBuffer;
    Graphics_QuadFlush(quad, blend);
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

static void _Graphics_InitTexture(OpenGLState *state, uint8 *data,
                                  int width, int height, int channels,
                                  std::string key)
{
    AssertA(state->texBinds < MAX_TEXTURES_COUNT, "Too many texture builds");
    uint tid = 0;
    int format = GL_RGB;
    glGenTextures(1, &tid);

    AssertA(tid > 0, "Unitialized texture id");

    glBindTexture(GL_TEXTURE_2D, tid);

    switch(channels){
        case 1:{
            format = GL_RED;
        } break;
        case 2:{
            format = GL_RG;
        } break;
        case 3:{
            format = GL_RGB;
        } break;
        case 4:{
            format = GL_RGBA;
        } break;

        default:{
            printf("Unknown texture format {%d}\n", channels);
            return;
        };
    }

    OpenGLCHK(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height,
                           0, format, GL_UNSIGNED_BYTE, data));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    OpenGLCHK(glGenerateMipmap(GL_TEXTURE_2D));
    glBindTexture(GL_TEXTURE_2D, 0);

    GlTextureId id = state->texBinds;
    state->textures[id].textureId = tid;
    state->textures[id].format = format;
    state->textures[id].width = width;
    state->textures[id].height = height;
    state->textureMap[key] = id;
    state->texBinds++;
}

static void _Graphics_BindTexture(OpenGLState *state, GlTextureId id, uint tid){
    AssertA(tid < MAX_TEXTURES_COUNT, "Invalid texture query");
    OpenGLTexture *tex = &state->textures[tid];

    //TODO: glBindTextureUnit is generating SIGSEGV, cannot figure out why
    //glBindTextureUnit((int)id, tex->textureId);

    //TODO: This however works
    char bindingname[32];
    snprintf(bindingname, sizeof(bindingname), "image%d", (int)id);
    glActiveTexture(GL_TEXTURE0 + (int)id);
    OpenGLCHK(glBindTexture(GL_TEXTURE_2D, tex->textureId));
    Shader_UniformInteger(state->imageShader, bindingname, (int)id);

    //printf("Binding %d = %s (%d)\n", id, tex->bindingname, tex->textureId);
}

static uint _Graphics_FetchTexture(OpenGLState *state, std::string strExt, int *off,
                                   std::string str = std::string())
{
    uint id = 0;
    // TODO: Maybe create an array so we can easily loop this thing
    if(strExt == ".cpp" || strExt == ".cc" || strExt == ".c"){
        id = state->textureMap[".cpp"];
    }else if(strExt == ".py"){
        id = state->textureMap[".py"];
    }else if(strExt == ".h" || strExt == ".hpp"){
        //id = state->textureMap[".cppheader"];
        //*off = 15;
        id = state->textureMap[".cpp"];
    }else if(strExt == ".cmake" || str == "CMakeLists.txt"){
        id = state->textureMap[".cmake"];
    }else if(strExt == ".glsl" || strExt == ".fs" || strExt == ".vs"
             || strExt == ".gl")
    {
        id = state->textureMap[".glsl"];
        *off = -25;
    }else if(strExt == ".cu" || strExt == ".cuh"){
        id = state->textureMap[".cu"];
        *off = -20;
    }else if(strExt == ".txt"){
        id = state->textureMap[".txt"];
        *off = 10;
    }else if(strExt == ".ttf"){
        id = state->textureMap[".ttf"];
        *off = 10;
    }else if(strExt == ".lit"){
        if(CurrentThemeIsLight()){
            id = state->textureMap[".lit_dark"];
        }else{
            id = state->textureMap[".lit_white"];
        }
    }else if(strExt == ".lit_dark"){
        id = state->textureMap[".lit_dark"];
    }else if(strExt == ".lit_white"){
        id = state->textureMap[".lit_white"];
    }
    else{
        // TODO: some unknown extension
        id = state->textureMap[".folder"];
    }
    return id;
}

OpenGLTexture Graphics_GetTextureInfo(uint id){
    if(id < MAX_TEXTURES_COUNT){
        return GlobalGLState.textures[id];
    }
    return OpenGLTexture();
}

FileExtension Graphics_TranslateFileExtension(FileExtension ext){
    FileExtension f = ext;
    if(ext == FILE_EXTENSION_LIT){
        if(CurrentThemeIsLight()){
            f = FILE_EXTENSION_LIT_DARK;
        }else{
            f = FILE_EXTENSION_LIT_WHITE;
        }
    }

    return f;
}

std::string Graphics_TranslateFileExtension(std::string ext){
    std::string f = ext;
    if(ext == ".lit"){
        if(CurrentThemeIsLight()){
            f = ".lit_dark";
        }else{
            f = ".lit_white";
        }
    }

    return f;
}

uint Graphics_FetchTextureFor(OpenGLState *state, FileEntry *e, int *off){
    uint id = 0;
    if(e->type == DescriptorFile){
        int p = GetFilePathExtension(e->path, e->pLen);
        *off = 0;
        if(p > 0){
            char *ext = &e->path[p];
            std::string str(e->path);
            std::string strExt(ext);
            strExt = Graphics_TranslateFileExtension(strExt);
            id = _Graphics_FetchTexture(state, strExt, off, str);
        }else{
            // NOTE: file wihtout extensions default to txt icons
            id = state->textureMap[".txt"];
            *off = 10;
        }
    }

    return id;
}

uint Graphics_FetchTextureFor(OpenGLState *state, FileExtension type, int *off){
    uint id = 0;
    type = Graphics_TranslateFileExtension(type);
    if(state->fileMap.find(type) != state->fileMap.end()){
        std::string key = state->fileMap[type];
        id = _Graphics_FetchTexture(state, key, off);
    }

    return id;
}

uint Graphics_FetchTextureFor(OpenGLState *state, std::string name, int *off){
    return _Graphics_FetchTexture(state, name, off, name);
}

void Graphics_BindImages(OpenGLState *state, OpenGLImageQuadBuffer *quad){
    for(uint i = 0; i < quad->units; i++){
        _Graphics_BindTexture(state, (GlTextureId)i, quad->textureIds[i]);
    }
}

void Graphics_TextureInit(OpenGLState *state, uint8 *image, uint len,
                          const char *key, FileExtension type)
{
    int x = 0, y = 0, n = 0;
    uint8 *data = ImageUtils_LoadPixels(image, len, &x, &y, &n);
    if(data){
        _Graphics_InitTexture(state, data, x, y, n, key);
        ImageUtils_FreePixels(data);
        if(type != FILE_EXTENSION_NONE){
            state->fileMap[type] = key;
        }
    }else{
        printf("Failed to load %s\n", key);
    }
}

void Graphics_AddEventHandler(double ival, std::function<bool(void)> eH){
    double stateInterval = Max(MIN_EVENT_SAMPLING_INTERVAL, ival);
    if(stateInterval < GlobalGLState.eventInterval){
        GlobalGLState.eventInterval = stateInterval;
    }

    Timing_Update();
    GlobalGLState.eventHandlers.push_back({eH, stateInterval, lastTime});
}

void CursorEventActionCallback(){
    GlobalGLState.cursorBlinkLastTime = GetElapsedTime();
    GlobalGLState.cursorBlinking = false;
    GlobalGLState.cursorVisible = true;
}

static bool BlinkingCursorEvent(){
    OpenGLState *state = &GlobalGLState;
    double currTime = GetElapsedTime();
    double interval = currTime - state->cursorBlinkLastTime;
    if(!state->cursorBlinking){
        if(interval >= 1.49){
            state->cursorBlinking = true;
        }
    }

    if(state->cursorBlinking){
        state->cursorVisible = !state->cursorVisible;
        state->cursorBlinkLastTime = currTime;
    }

    return state->enableCursorBlink;
}

void Graphics_ToogleCursorBlinking(){
    TOOGLE_VAR(GlobalGLState.enableCursorBlink);
    if(GlobalGLState.enableCursorBlink){
        GlobalGLState.cursorBlinkKeyboardHandle =
                KeyboardRegisterActiveEvent(CursorEventActionCallback);
        GlobalGLState.cursorBlinkMouseHandle =
                AppRegisterOnMouseEventCallback(CursorEventActionCallback);

        Graphics_AddEventHandler(0.5, BlinkingCursorEvent);
    }else{
        KeyboardReleaseActiveEvent(GlobalGLState.cursorBlinkKeyboardHandle);
        AppReleaseOnMouseEventCallback(GlobalGLState.cursorBlinkMouseHandle);
    }
}

void UpdateEventsAndHandleRequests(int animating, double frameInterval){
    OpenGLState *state = &GlobalGLState;
    // check if we are handling custom events outside
    // the UI that require live update, for these we
    // can pool but they will overconsume our CPU
    // so cap at specific interval as they don't really need high fps
    // but without it we would need to integrate x11 pooling
    // with concurrent events, yikes...
    if(state->eventHandlers.size() > 0 || animating){
        // pool the UI first
        PoolEventsX11();
        if(state->eventHandlers.size() > 0){
            double pTime = GetElapsedTime();
            // check if we actually have to any work
            if(pTime - state->lastEventTime < state->eventInterval){
                // we sampled at a smaller interval, check against max sampling rate
                // so we don't waste CPU on high frequency sampling
                double dif = state->maxSamplingRate - frameInterval;
                if(dif > 0){
                    int idif = dif * 1000.0;
                    //printf("FPS %g - Dif = %d ( %g )\n", 1.f / frameInterval, idif, dif);
                    std::this_thread::sleep_for(std::chrono::milliseconds(idif));
                }
                return;
            }

            // pool the events and update next event interval in case anyone bails out
            std::vector<EventHandler>::iterator it;
            double expected_ival = Infinity;
            bool any_rems = false;
            state->lastEventTime = pTime;

            for(it = state->eventHandlers.begin(); it != state->eventHandlers.end(); ){
                EventHandler handler = *it;
                double interval = pTime - handler.lastCalled;
                if(interval > handler.interval){
                    if(!handler.fn()){
                        it = state->eventHandlers.erase(it);
                        any_rems = true;
                    }else{
                        handler.lastCalled = pTime;
                        *it = handler;
                        it++;
                    }
                }else{
                    it++;
                }

                expected_ival = Min(expected_ival, handler.interval);
            }

            // in case someone exited we need to update the iterval
            // for the next one
            if(any_rems && state->eventHandlers.size() > 0){
                state->eventInterval = expected_ival;
            }
        }
    }else{
        // if we are not handling custom events than simply wait for the
        // next UI event. This is fine because Cody is UI-driven
        WaitForEventsX11();
    }
}

void Graphics_RequestClose(WidgetWindow *w){
    OpenGLState *state = &GlobalGLState;
    std::vector<std::shared_ptr<WidgetWindow>>::iterator it;
    if(w){
        for(it = state->widgetWindows.begin(); it != state->widgetWindows.end(); ){
            WidgetWindow *ptr = it->get();
            if(w == ptr){
                it = state->widgetWindows.erase(it);
                break;
            }
        }
    }
}

/*
* TODO: Implement a hex editor for binary files so we can open binary files.
*       it shouldn't be too dificult, we mostly have everything already.
*/
int OpenGLRenderMainWindow(WidgetRenderContext *wctx){
    OpenGLState *state = &GlobalGLState;
    OpenGLFont *font = &state->font;
    int animating = 0;
    ThemeVisuals visuals;
    AppUpdateViews();

    CurrentThemeGetVisuals(visuals);

    glUseProgram(font->shader.id);
    Shader_UniformFloat(font->shader, "contrast", visuals.contrast);
    Shader_UniformFloat(font->shader, "brightness", visuals.brightness);
    Shader_UniformFloat(font->shader, "saturation", visuals.saturation);

    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View *view = iterator.value->view;
        BufferView *bView = View_GetBufferView(view);
        RenderList *pipeline = View_GetRenderPipeline(view);
        BufferView_UpdateCursorNesting(bView);

        // Sets the alpha for the current view rendering stages, makes the dimm effect

        // TODO: quick hack to disable dimm for build buffer
        int dim = view->isActive ? 0 : 1;
        if(bView->lineBuffer){
            if(bView->lineBuffer->filePathSize == 0)
                dim = 0;
        }

        SetAlpha(dim);
        bView->is_transitioning = 0;

        if(BufferView_IsVisible(bView)){
            OpenGLResetCursors(state);
            for(uint s = 0; s < pipeline->stageCount; s++){
                state->model = state->scale; // reset model
                animating |= pipeline->stages[s].renderer(view, state,
                                                          defaultTheme, wctx->dt);
            }
        }

        // acount for chromatic transition
        animating |= bView->is_transitioning;
        if(bView->is_transitioning){
            LineBuffer *lineBuffer = BufferView_GetLineBuffer(bView);
            LineBuffer_AdvanceCopySection(lineBuffer, wctx->dt);
        }
        ViewTree_Next(&iterator);
    }
    return animating;
}

void OpenGLEntry(){
    BufferView *bufferView = AppGetActiveBufferView();
    OpenGLState *state = &GlobalGLState;
    WidgetRenderContext wctx = { .state = state };

    _OpenGLStateInitialize(state);
    OpenGLInitialize(state);
    OpenGLFontSetup(state);

    _OpenGLUpdateProjections(state, state->width, state->height);
    _OpenGLInitGlobalWidgets(state);

    BufferView_CursorTo(bufferView, 0);
    Timing_Update();
    state->lastEventTime = lastTime;

    //KeyboardRegisterActiveEvent(CursorEventActionCallback);
    //Graphics_AddEventHandler(0.5, BlinkingCursorEvent);

    //_debugger_memory_usage();
    //state->widgetWindows.push_back(std::shared_ptr<WidgetWindow>(new PopupWindow));

    while(!WindowShouldCloseX11(state->window)){
        MakeContextX11(state->window);
        double currTime = GetElapsedTime();
        double dt = currTime - lastTime;
        int animating = 0;
        wctx.dt = dt;

        lastTime = currTime;
        state->bErrors.clear();
        FetchBuildErrors(state->bErrors);

        // render main window
        animating |= OpenGLRenderMainWindow(&wctx);
        if(state->gWidgets.wwindow->WidgetCount() > 0){
            animating |= state->gWidgets.wwindow->DispatchRender(&wctx);
        }

        SwapBuffersX11(state->window);

        // render popups and other windows
        for(std::shared_ptr<WidgetWindow> &sww : state->widgetWindows){
            WidgetWindow *ptr = sww.get();
            if(ptr){
                animating |= ptr->DispatchRender(&wctx);
            }
        }

        UpdateEventsAndHandleRequests(animating, dt);
    }

    if(Dbg_IsRunning()){
        DbgApp_Exit();
        // sleep to make sure the debugger has enough time
        // to send the message, response might take longer
        // because it depends on its state, however it should
        // not be our problem anymore as we are not going to answer it
        sleep(1);
    }

    state->widgetWindows.clear();
    delete state->gWidgets.wdbgBt;
    delete state->gWidgets.wwindow;
    delete state->gWidgets.wdbgVw;

    DestroyWindowX11(state->window);
    TerminateX11();
    state->running = 0;
    state->window = nullptr;
    FinishExecutor();
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
        GlobalGLState.eventInterval = 0.008333; // 1/120
        std::thread(OpenGLEntry).detach();
    }
#endif
}
