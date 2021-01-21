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
    Float *vertexBuffer;
    Float *colorBuffer;
    uint size;
    uint length;
}Quad;

typedef struct{
    GLFWwindow *window;
    OpenGLFont font;
    Float cursorQuad[12];
    uint vertexArray;
    uint vertexBuffer;
    int running, width, height;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
}OpenGLState;

BufferView bufferView;
static OpenGLState GlobalGLState;

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

void window_size_callback(GLFWwindow* window, int width, int height){
    Float range = width * 2.0f;
    Float zNear = -range;
    Float zFar = range;
    
    GlobalGLState.width = width;
    GlobalGLState.height = height;
    OpenGLFont *font = &GlobalGLState.font;
    GlobalGLState.projection = Orthographic(0.0f, width, height, 0.0f, zNear, zFar);
    GlobalGLState.scale = Scale(font->fontMath.reduceScale, 
                                font->fontMath.reduceScale, 0);
    
    BufferView_SetView(&bufferView, font->fontMath.fontSizeAtDisplay,
                       GlobalGLState.height);
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

void OpenGLFontUse(OpenGLFont *font){
    glUseProgram(font->shader.id);
}

void OpenGLInitialize(OpenGLState *state){
    DEBUG_MSG("Initializing OpenGL graphics\n");
    int width = state->width;
    int height = state->height;
    int rc = glfwInit();
    
    AssertA(rc == GLFW_TRUE, "Failed to initialize GLFW");
    glfwWindowHint(GLFW_SAMPLES, 16);
    //glfwWindowHint(GLFW_RESIZABLE, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    state->window = glfwCreateWindow(width, height, "Mayhem - 0.0.1", NULL, NULL);
    AssertA(state->window != NULL, "Failed to open window");
    
    glfwMakeContextCurrent(state->window);
    
    AssertA(gladLoadGL() != 0, "Failed to load OpenGL pointers");
    GLenum error = glGetError();
    AssertA(error == GL_NO_ERROR, "Failed to setup glfw context");
    
    glfwSetWindowSizeCallback(state->window, window_size_callback);
    
    glGenVertexArrays(1, &state->vertexArray);
    glBindVertexArray(state->vertexArray);
    
    glGenBuffers(1, &state->vertexBuffer);
}

void Graphics_RenderRectangle(vec2ui left, vec2ui right, vec4f color){
    
}

vec2f OpenGLRenderCursor(OpenGLState *state, Buffer *buffer, vec2ui cursor,
                         Float cursorOffset, vec2ui visibleLines,
                         int *prevGlyph)
{
    int nId = 0;
    int previousGlyph = -1;
    char *ptr = buffer->data;
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
    
    //printf("Cursor x-box %g %g '%c'\n", x0, x1, ptr[cursor.y]);
    
    // Lower triangle
    state->cursorQuad[nId * 2 + 0] = x0; state->cursorQuad[nId * 2 + 1] = y0; nId++;
    state->cursorQuad[nId * 2 + 0] = x1; state->cursorQuad[nId * 2 + 1] = y1; nId++;
    state->cursorQuad[nId * 2 + 0] = x1; state->cursorQuad[nId * 2 + 1] = y0; nId++;
    
    // Upper triangle
    state->cursorQuad[nId * 2 + 0] = x0; state->cursorQuad[nId * 2 + 1] = y0; nId++;
    state->cursorQuad[nId * 2 + 0] = x0; state->cursorQuad[nId * 2 + 1] = y1; nId++;
    state->cursorQuad[nId * 2 + 0] = x1; state->cursorQuad[nId * 2 + 1] = y1; nId++;
    
    glBindVertexArray(state->vertexArray);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, state->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, nId * 2 * sizeof(Float), state->cursorQuad,
                 GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    
    glDrawArrays(GL_TRIANGLES, 0, nId);
    
    glDisableVertexAttribArray(0);
    *prevGlyph = previousGlyph;
    return vec2f(x0, y0);
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
    if(buffer->count > 0){
        uint pos = 0;
        uint at = 0;
        for(int i = 0; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            //TODO: Precompute spacing size?
            while(pos != token->position){
                //TODO: Check what character ' ' || '\r' || '\t'
                char v = buffer->data[pos];
                if(v == '\r' || v == '\n'){
                    pos++;
                    continue;
                }
                
                if(v == '\t'){
                    //TODO: Check spacing offset configuration
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

/*
* TODO: We need to try rendering only the rect that is visible
*       and instead of rendering only when changes are made
*       render everything at 60fps, it should look like a continuous
*       image and reduce the cost of rendering large files.
*/
void OpenGLEntry(){
    _OpenGLStateInitialize(&GlobalGLState);
    OpenGLInitialize(&GlobalGLState);
    OpenGLFontSetup(&GlobalGLState.font);
    
    Float background = 0.;
    Float ones[] = {1,1,1,1};
    Float black[] = {background, background, background, 0};
    Float width  = (Float)GlobalGLState.width;
    Float height = (Float)GlobalGLState.height;
    Float range = width * 2.0f;
    
    Float zNear = -range;
    Float zFar = range;
    
    OpenGLFont *font = &GlobalGLState.font;
    GlobalGLState.projection = Orthographic(0.0f, width, height, 0.0f, zNear, zFar);
    GlobalGLState.scale = Scale(font->fontMath.reduceScale, 
                                font->fontMath.reduceScale, 0);
    
    BufferView_SetView(&bufferView, font->fontMath.fontSizeAtDisplay,
                       GlobalGLState.height);
    
    vec2ui lineP = 0;
    int cc = 1;
    double lastFrameTime = glfwGetTime();
    int runned = 0;
    while(!glfwWindowShouldClose(GlobalGLState.window)){
        if(GlobalGLState.window){
            runned ++;
            //TODO: Render
            vec3i backgroundColor = defaultTheme.backgroundColor;
            Float fcol[] = {(Float)backgroundColor.x / 255.0f,
                (Float)backgroundColor.y / 255.0f,
                (Float)backgroundColor.z / 255.0f,
                0
            };
            
            glViewport(0, 0, GlobalGLState.width, GlobalGLState.height);
            glClearBufferfv(GL_COLOR, 0, fcol);
            glClearBufferfv(GL_DEPTH, 0, ones);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            
            OpenGLFontUse(font);
            
            //Float lineHeight = font->fontMath.fontSizeAtDisplay;
            //Transform translate = GlobalGLState.scale * Translate(0.0f, -lineHeight * 0.5, 0.0f);
            
            Shader_UniformMatrix4(font->shader, "projection", &GlobalGLState.projection.m);
            Shader_UniformMatrix4(font->shader, "modelView", &GlobalGLState.scale.m);
            //Shader_UniformMatrix4(font->shader, "modelView", &translate.m);
            
            Float x = 0.0f;
            Float y = 0.0f;
            
            fonsClearState(font->fsContext);
            
            fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
            fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            
            lineP = BufferView_GetCursorPosition(&bufferView);
            lineP.x += cc;
            //if(lineP.x == 0) cc = 1;
            if(lineP.x == 1000) cc = -1;
            
            BufferView_CursorTo(&bufferView, 20);
            vec2ui lines = BufferView_GetViewRange(&bufferView);
            vec2ui cursor = BufferView_GetCursorPosition(&bufferView);
            
            clock_t start = clock();
            Float lineOffset = 0;
            for(int i = lines.x; i < lines.y; i++){
                Float tmp = 0;
                OpenGLRenderLine(&bufferView.lineBuffer, font, x, y, i, tmp);
                if(i == cursor.x){
                    lineOffset = tmp;
                }
                x = 0.0f;
                y += font->fontMath.fontSizeAtRenderCall;
            }
            clock_t end = clock();
            double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
            //printf("Took %g %u\n\n", taken, lineP.x);
            
            fonsStashFlush(font->fsContext);
            glDisable(GL_BLEND);
            y += font->fontMath.fontSizeAtRenderCall;
            
            Buffer *buffer = &bufferView.lineBuffer.lines[cursor.x];
            glUseProgram(font->cursorShader.id);
            Shader_UniformMatrix4(font->cursorShader, "projection",
                                  &GlobalGLState.projection.m);
            Shader_UniformMatrix4(font->cursorShader, "modelView",
                                  &GlobalGLState.scale.m);
            
            int pGlyph = -1;
            vec2f p = OpenGLRenderCursor(&GlobalGLState, buffer, cursor, 
                                         lineOffset, lines, &pGlyph);
            
            if(cursor.y < buffer->count){
                glEnable(GL_BLEND);
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
                OpenGLFontUse(font);
                Shader_UniformMatrix4(font->shader, "projection", &GlobalGLState.projection.m);
                Shader_UniformMatrix4(font->shader, "modelView", &GlobalGLState.scale.m);
                
                char *chr = &buffer->data[cursor.y];
                float f = fonsStashMultiTextColor(font->fsContext, p.x, p.y, 
                                                  glfonsRGBA(0, 0, 0, 255),
                                                  chr, chr+1, &pGlyph);
                
                fonsStashFlush(font->fsContext);
            }
        }
        
        glfwSwapBuffers(GlobalGLState.window);
        glfwPollEvents();
        //glfwWaitEvents();
        usleep(100000);
        double curr = glfwGetTime();
        //double diff = curr - lastFrameTime;
        //printf("Frame interval: %g, fps: %g\n", diff, 1.0 / diff);
        lastFrameTime = curr;
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    glfwDestroyWindow(GlobalGLState.window);
    glfwTerminate();
    
    GlobalGLState.running = 0;
    GlobalGLState.window = nullptr;
}

void Graphics_Initialize(){
    // TODO: Refactor
    if(GlobalGLState.running == 0){
        GlobalGLState.running = 1;
        std::thread(OpenGLEntry).detach();
    }
}
