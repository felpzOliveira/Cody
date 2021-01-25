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
//NOTE: We do have access to glad should we keep it or imlement it?
#include <glad/glad.h>

#include <x11_display.h>
#include <iostream>

//NOTE: Since we already modified fontstash source to reduce draw calls
//      we might as well embrace it
#define FONS_VERTEX_COUNT 8192
#define FONTSTASH_IMPLEMENTATION
#include <fontstash.h>

//TODO: Refactor these, possibly remove, dev render code only
#define GLFONTSTASH_IMPLEMENTATION
#include <gl3corefontstash.h> //TODO: Make better backend this one is kinda silly
#include <GLFW/glfw3.h> // TODO: This one is going to be annoying to replace
#include <unistd.h> // TODO: I want to use this on Windows so we need to erase this

#define MODULE_NAME "Render"

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
}Quad;

typedef struct{
    WindowX11 *window;
    OpenGLFont font;
    Quad quadBuffer;
    int running, width, height;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
}OpenGLState;

BufferView bufferView;
static OpenGLState GlobalGLState;

/*
* NOTE: IMPORTANT!
* Remenber that we render things in higher scale and than
* downscale with a projection, so if you want to specify 
* a rectangle or whatever in screen space [0, 0] x [width, height]
* you need to multiply the coordinates by the inverse scale:
*               font->fontMath.invReduceScale
*/

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
                         font->fontMath.reduceScale, 0);
    BufferView_SetView(&bufferView, font->fontMath.fontSizeAtDisplay,
                       state->height);
}

void character_callback(GLFWwindow* window, unsigned int codepoint){
    char v[6] = {0, 0, 0, 0, 0, 0};
    int n = CodepointToString((int)codepoint, v);
    AssertA(n > 0, "Invalid codepoint given");
    printf("Received: %s\n", v);
}

void chars_mods(GLFWwindow* window, unsigned int codepoint, int mods){
    char v[6] = {0, 0, 0, 0, 0, 0};
    int n = CodepointToString((int)codepoint, v);
    //printf("Got: %s\n", v);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    int keySymsPerkeyCode=0;
    Display *display = glfwX11GetDisplayHandle();
    KeySym *keySyms( XGetKeyboardMapping(display, scancode, 1, &keySymsPerkeyCode)),
    *keySym = keySyms;
#if 1
    while (keySymsPerkeyCode--  &&  *keySym != NoSymbol) {
        std::cout << *keySym << std::endl;
        ++keySym;
    }
#endif
    
    if(mods & GLFW_MOD_CONTROL){
        printf("ctrl\n");
    }
    
    if(mods & GLFW_MOD_SHIFT){
        printf("shift\n");
    }
}

void window_size_callback(GLFWwindow* window, int width, int height){
    _OpenGLUpdateProjections(&GlobalGLState, width, height);
}


void OpenGLFontSetup(OpenGLFont *font){
    uint filesize = 0;
    FontMath *fMath = nullptr;
    char *fontfileContents = nullptr;
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
    Quad *quad = &state->quadBuffer;
    int width = state->width;
    int height = state->height;
    
    InitializeX11();
    SetSamplesX11(16);
    SetOpenGLVersionX11(3, 3);
    state->window = CreateWindowX11(width, height, "Cody - 0.0.1");
    
    AssertA(gladLoadGL() != 0, "Failed to load OpenGL pointers");
    GLenum error = glGetError();
    AssertA(error == GL_NO_ERROR, "Failed to setup glfw context");
    
#if 0
    glfwSetWindowSizeCallback(state->window, window_size_callback);
    glfwSetCharCallback(state->window, character_callback);
    glfwSetKeyCallback(state->window, key_callback);
    glfwSetCharModsCallback(state->window, chars_mods);
#endif
    
    int n = 1024;
    quad->vertex = AllocatorGetN(Float, n);
    quad->colors = AllocatorGetN(Float, 4 * n);
    quad->size = n;
    quad->length = 0;
    
    glGenVertexArrays(1, &quad->vertexArray);
    glBindVertexArray(quad->vertexArray);
    
    glGenBuffers(1, &quad->vertexBuffer);
    glGenBuffers(1, &quad->colorBuffer);
}

void Graphics_QuadPushVertex(OpenGLState *state, Float x, Float y, vec4f color){
    Quad *quad = &state->quadBuffer;
    quad->vertex[2 * quad->length + 0] = x;
    quad->vertex[2 * quad->length + 1] = y;
    
    quad->colors[4 * quad->length + 0] = color.x;
    quad->colors[4 * quad->length + 1] = color.y;
    quad->colors[4 * quad->length + 2] = color.z;
    quad->colors[4 * quad->length + 3] = color.w;
    
    quad->length ++;
}

void Graphics_QuadPush(OpenGLState *state, vec2ui left, vec2ui right, vec4f color){
    Float x0 = left.x, y0 = left.y, x1 = right.x, y1 = right.y;
    Graphics_QuadPushVertex(state, x0, y0, color);
    Graphics_QuadPushVertex(state, x1, y1, color);
    Graphics_QuadPushVertex(state, x1, y0, color);
    
    Graphics_QuadPushVertex(state, x0, y0, color);
    Graphics_QuadPushVertex(state, x0, y1, color);
    Graphics_QuadPushVertex(state, x1, y1, color);
}

void Graphics_QuadFlush(OpenGLState *state){
    Quad *quad = &state->quadBuffer;
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
    quad->length = 0;
}

vec4f OpenGLRenderCursor(OpenGLState *state, Buffer *buffer, vec2ui cursor,
                         Float cursorOffset, vec2ui visibleLines,
                         int *prevGlyph)
{
    int nId = 0;
    int previousGlyph = -1;
    char *ptr = buffer->data;
    Quad *quad = &state->quadBuffer;
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    
    if(cursor.y > 0){
        x0 = fonsComputeStringAdvance(state->font.fsContext, ptr, cursor.y, &previousGlyph);
    }
    
    x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[cursor.y],
                                       1, &previousGlyph);
    
    y0 = (cursor.x - visibleLines.x) * state->font.fontMath.fontSizeAtRenderCall;
    y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
    
    x0 += cursorOffset;
    x1 += cursorOffset;
    
    if(quad->length + 6 > quad->size){
        Graphics_QuadFlush(state);
    }
    
    Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), vec4f(0,1,0,1));
    
    *prevGlyph = previousGlyph;
    
    return vec4f(x0, y0, x1, y1);
}

void OpenGLRenderLine(LineBuffer *lineBuffer, OpenGLFont *font,
                      Float &x, Float &y, uint lineNr,
                      Float &lineStartX /* TODO: Add more info */)
{
    Buffer *buffer = &lineBuffer->lines[lineNr];
    char linen[32];
    int previousGlyph = -1;
    int spacing = DigitCount(lineBuffer->lineCount);
    int ncount = DigitCount(lineNr);
    memset(linen, ' ', spacing-ncount);
    snprintf(&linen[spacing-ncount], 32, "%u ", lineNr);
    
    x = fonsStashMultiTextColor(font->fsContext, x, y, glfonsRGBA(0, 180, 255, 255), 
                                linen, NULL, &previousGlyph);
    lineStartX = x;
    //TODO: This is where we would wrap?
    if(buffer->count > 0){
        uint pos = 0;
        uint at = 0;
        for(int i = 0; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            while(pos != token->position){
                char v = buffer->data[pos];
                if(v == '\r' || v == '\n'){
                    pos++;
                    continue;
                }
                
                if(v == '\t'){
                    x += fonsComputeStringAdvance(font->fsContext, "    ", 4,
                                                  &previousGlyph);
                    pos ++;
                    continue;
                }
                
                if(v == ' '){
                    x += fonsComputeStringAdvance(font->fsContext, " ", 1,
                                                  &previousGlyph);
                    pos++;
                    continue;
                }
                
                if(v == 0){
                    return;
                }
                
                printf("Position %u  ==> Token %u, char \'%c\'\n", pos, token->position, v); 
                AssertA(0, "Invalid character");
            }
            
            if(token->position < buffer->count){
                char *p = &buffer->data[token->position];
                char *e = &buffer->data[token->position + token->size];
                vec3i col = GetColor(&defaultTheme, token->identifier);
                pos += token->size;
                x = fonsStashMultiTextColor(font->fsContext, x, y, glfonsRGBA(col.x, col.y,
                                                                              col.z, 255),
                                            p, e, &previousGlyph);
            }
        }
    }
}

Float Graphics_RenderTextBlock(OpenGLState *state, LineBuffer *lineBuffer,
                               vec2ui left, vec2ui right)
{
    OpenGLFont *font = &state->font;
    vec2ui lines, cursor;
    Float lineOffset = 0;
    Float x = 0.0f;
    Float y = 0.0f;
    
    glViewport(left.x, left.y, right.x, right.y);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);
    
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    lines = BufferView_GetViewRange(&bufferView);
    cursor = BufferView_GetCursorPosition(&bufferView);
    
    for(int i = lines.x; i < lines.y; i++){
        Float tmp = 0;
        OpenGLRenderLine(&bufferView.lineBuffer, font, x, y, i, tmp);
        if(i == cursor.x){
            lineOffset = tmp;
        }
        x = 0.0f;
        y += font->fontMath.fontSizeAtRenderCall;
    }
    
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
    return lineOffset;
}

void Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                   Float lineAlignment, Float lineSpan)
{
    int pGlyph = -1;
    OpenGLFont *font = &state->font;
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    
    Buffer *buffer = &bufferView->lineBuffer.lines[cursor.x];
    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
    
    vec2ui lines = BufferView_GetViewRange(bufferView);
    vec4f p = OpenGLRenderCursor(state, buffer, cursor, lineAlignment, lines, &pGlyph);
    
    Float g = 0.8705882; //TODO: add this to theme
    Graphics_QuadPush(state, vec2ui(lineAlignment, p.y), 
                      vec2ui(lineAlignment+lineSpan, p.w), vec4f(g, g, 2 * g, 0.1));
    Graphics_QuadFlush(state);
    
    /* Redraw whatever character we are on top of */
    if(cursor.y < buffer->count){
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        
        glUseProgram(font->shader.id);
        Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
        Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);
        
        char *chr = &buffer->data[cursor.y];
        float f = fonsStashMultiTextColor(font->fsContext, p.x, p.y, 
                                          glfonsRGBA(0, 0, 0, 255),
                                          chr, chr+1, &pGlyph);
        
        fonsStashFlush(font->fsContext);
    }
}

void OpenGLEntry(){
    Float ones[] = {1,1,1,1};
    OpenGLFont *font = &GlobalGLState.font;
    
    _OpenGLStateInitialize(&GlobalGLState);
    OpenGLInitialize(&GlobalGLState);
    OpenGLFontSetup(&GlobalGLState.font);
    
    _OpenGLUpdateProjections(&GlobalGLState, GlobalGLState.width, 
                             GlobalGLState.height);
    
    int lineCC = 1;
    while(1/*!glfwWindowShouldClose(GlobalGLState.window)*/){
        vec2ui offset = vec2ui(0, 0);
        Float scaledWidth = 0;
        Float width = GlobalGLState.width;
        Float height = GlobalGLState.height;
        
        scaledWidth = width * font->fontMath.invReduceScale;
        
        vec3i backgroundColor = defaultTheme.backgroundColor;
        Float fcol[] = {(Float)backgroundColor.x / 255.0f,
            (Float)backgroundColor.y / 255.0f,
            (Float)backgroundColor.z / 255.0f, 0
        };
        
        glViewport(0, 0, width, height);
        glClearBufferfv(GL_COLOR, 0, fcol);
        glClearBufferfv(GL_DEPTH, 0, ones);
        
        vec2ui cursor = BufferView_GetCursorPosition(&bufferView);
        if(cursor.x > 900) lineCC = -1;
        if(cursor.x < 2) lineCC = 1;
        
        cursor.x += lineCC;
        
        BufferView_CursorTo(&bufferView, 100);
        
        Float lineOffset = Graphics_RenderTextBlock(&GlobalGLState, 
                                                    &bufferView.lineBuffer, offset,
                                                    vec2ui(width, height));
        
        Graphics_RenderCursorElements(&GlobalGLState, &bufferView, 
                                      lineOffset, scaledWidth);
        
        SwapBuffersX11(GlobalGLState.window);
        PoolEventsX11();
        
        //glfwSwapBuffers(GlobalGLState.window);
        //glfwPollEvents();
        //glfwWaitEvents();
        usleep(10000);
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    //glfwDestroyWindow(GlobalGLState.window);
    //glfwTerminate();
    
    GlobalGLState.running = 0;
    GlobalGLState.window = nullptr;
}

int Graphics_IsRunning(){
    return GlobalGLState.running;
}

void Graphics_Initialize(){
    // TODO: Refactor
    if(GlobalGLState.running == 0){
        GlobalGLState.running = 1;
        std::thread(OpenGLEntry).detach();
    }
}
