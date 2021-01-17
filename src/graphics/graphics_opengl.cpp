#include <graphics.h>
#include <types.h>
#include <thread>
#include <shaders.h>
#include <transform.h>
#include <utilities.h>
//NOTE: We do have access to glad should we keep it or imlement it?
#include <glad/glad.h>

//NOTE: Since we already modified fontstash source to reduce draw calls
//      we might as well embrace it
#define FONS_VERTEX_COUNT 2048
#define FONTSTASH_IMPLEMENTATION
#include <fontstash.h>

//TODO: Refactor these, possibly remove, dev render code only
#define GLFONTSTASH_IMPLEMENTATION
#include <gl3corefontstash.h> //TODO: Make better backend this one is kinda silly
#include <GLFW/glfw3.h> // TODO: This one is going to be annoying to replace
#include <unistd.h> // TODO: I want to use this on Windows so we need to erase this

#define MODULE_NAME "Render"

typedef struct{
    GLFWwindow *window;
    int running, width, height;
}OpenGLThread;

#define OPENGL_THREAD_INITIALIZER {.window = nullptr, .running = 0,\
.width=1366, .height=720}

static OpenGLThread glThreadInfo = OPENGL_THREAD_INITIALIZER;

void OpenGLLoadShaders(){
    Shader shader0 = SHADER_INITIALIZER;
    Shader shader1 = SHADER_INITIALIZER;
    Shader shader2 = SHADER_INITIALIZER;
    uint vs, fs0, fs1, fs2;
    
    const char *v0 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text.v.glsl";
    const char *f0 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text.f.glsl";
    const char *f1 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text_sdf.f.glsl";
    const char *f2 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text_sdf_effects.f.glsl";
    
    vs  = Shader_CompileFile(v0, SHADER_TYPE_VERTEX);
    fs0 = Shader_CompileFile(f0, SHADER_TYPE_FRGMENT);
    fs1 = Shader_CompileFile(f1, SHADER_TYPE_FRGMENT);
    fs2 = Shader_CompileFile(f2, SHADER_TYPE_FRGMENT);
    
    Shader_Create(shader0, vs, fs0);
    Shader_Create(shader1, vs, fs1);
    Shader_Create(shader2, vs, fs2);
    
    DEBUG_MSG("IDS: %u, %u, %u\n", shader0.id, shader1.id, shader2.id);
}

void OpenGLEntry(){
    DEBUG_MSG("Initializing OpenGL graphics\n");
    int rc = glfwInit();
    AssertA(rc == GLFW_TRUE, "Failed to initialize GLFW");
    
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_RESIZABLE, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glThreadInfo.window = glfwCreateWindow(glThreadInfo.width, glThreadInfo.height,
                                           "Mayhem - 0.0.1", NULL, NULL);
    AssertA(glThreadInfo.window != NULL, "Failed to open window");
    glfwMakeContextCurrent(glThreadInfo.window);
    
    AssertA(gladLoadGL() != 0, "Failed to load OpenGL pointers");
    
    GLenum error = glGetError();
    AssertA(error == GL_NO_ERROR, "Failed to setup glfw context");
    
    //OpenGLLoadShaders();
    Shader shader = SHADER_INITIALIZER;
    Shader shaderSdf = SHADER_INITIALIZER;
    const char *v0 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text.v.glsl";
    const char *f0 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text.f.glsl";
    const char *f1 = "/home/felipe/Documents/sdf_text_sample/assets/shaders/text_sdf.f.glsl";
    uint vs  = Shader_CompileFile(v0, SHADER_TYPE_VERTEX);
    uint fs0 = Shader_CompileFile(f0, SHADER_TYPE_FRGMENT);
    uint fs1 = Shader_CompileFile(f1, SHADER_TYPE_FRGMENT);
    
    Shader_Create(shader, vs, fs0);
    Shader_Create(shaderSdf, vs, fs1);
    DEBUG_MSG("Shader id: %u %u\n", shader.id, shaderSdf.id);
    
    FONScontext *fsContext = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
    const char *droidSansFilename = "/home/felipe/Documents/sdf_text_sample/assets/fonts/droid/liberation-mono.ttf";
    
    uint fileSize = 0;
    char *fontContent = GetFileContents(droidSansFilename, &fileSize);
    AssertA(fontContent != nullptr, "Failed to read font file");
    
    FONSsdfSettings noSdf = {0};
    noSdf.sdfEnabled = 0;
    
    int fontNormal = fonsAddFontSdfMem(fsContext, "DroidSans", (uint8 *)fontContent, 
                                       fileSize, 0, noSdf);
    AssertA(fontNormal != FONS_INVALID, "Failed to create font");
    
    FONSsdfSettings basicSdf = {0};
    basicSdf.sdfEnabled = 1;
    basicSdf.onedgeValue = 127;
    basicSdf.padding = 1;
    basicSdf.pixelDistScale = 62.0;
    
    int fontSdf = fonsAddFontSdfMem(fsContext, "DroidSansSdf", (uint8 *)fontContent, 
                                    fileSize, 0, basicSdf);
    
    AssertA(fontSdf != FONS_INVALID, "Failed to create sdf font");
    
    DEBUG_MSG("Font rv: %d... %d\n", fontNormal, fontSdf);
    
    Float background = 0.;
    Float ones[] = {1,1,1,1};
    Float black[] = {background, background, background, 0};
    Float width  = (Float)glThreadInfo.width;
    Float height = (Float)glThreadInfo.height;
    Float range = width * 2.0f;
    
    Float zNear = -range;
    Float zFar = range;
    
    Transform projection = Orthographic(0.0f, width, height, 0.0f, zNear, zFar);
    Matrix4x4 identity;
    Float fontSize = 20.0f;
    
    while(!glfwWindowShouldClose(glThreadInfo.window)){
        if(glThreadInfo.window){
            //TODO: Render
            glViewport(0, 0, glThreadInfo.width, glThreadInfo.height);
            glClearBufferfv(GL_COLOR, 0, black);
            glClearBufferfv(GL_DEPTH, 0, ones);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            
            glUseProgram(shader.id);
            Shader_UniformMatrix4(shader, "projection", &projection.m);
            Shader_UniformMatrix4(shader, "modelView", &identity);
            
            Float lineHeight = 0.0f;
            Float x = 0.0f;
            Float y = 0.0f;
            
            fonsClearState(fsContext);
            fonsSetFont(fsContext, fontNormal);
            fonsSetSize(fsContext, fontSize);
            fonsSetAlign(fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsVertMetrics(fsContext, NULL, NULL, &lineHeight);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 0, 0, 255), 
                                        "Lorem ", NULL);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 255, 0, 255),
                                        "ipsum ", NULL);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 0, 0, 255),
                                        "-> dolor sit amet (not SDF)",NULL);
            
            fonsStashFlush(fsContext);
            
            y += lineHeight;
            x = 0.0f;
            
            glUseProgram(shaderSdf.id);
            Shader_UniformMatrix4(shaderSdf, "projection", &projection.m);
            Shader_UniformMatrix4(shaderSdf, "modelView", &identity);
            
            fonsClearState(fsContext);
            fonsSetFont(fsContext, fontSdf);
            fonsSetSize(fsContext, fontSize);
            fonsSetAlign(fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            fonsVertMetrics(fsContext, NULL, NULL, &lineHeight);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 0, 0, 255),
                                        "Lorem ", NULL);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 255, 0, 255),
                                        "ipsum ", NULL);
            
            x = fonsStashMultiTextColor(fsContext, x, y, glfonsRGBA(255, 0, 0, 255),
                                        "dolor sit amet (SDF)", NULL);
            
            fonsStashFlush(fsContext);
            y += lineHeight;
            
            glDisable(GL_BLEND);
        }
        
        glfwSwapBuffers(glThreadInfo.window);
        glfwPollEvents();
        usleep(100000);
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    glfwDestroyWindow(glThreadInfo.window);
    glfwTerminate();
    
    glThreadInfo.running = 0;
    glThreadInfo.window = nullptr;
}

void Graphics_InitializeOpenGL(Graphics_Opts *opts){
    int rc = 0;
    AssertA(opts != nullptr, "Invalid OpenGL configuration");
    AssertA(opts->is_opengl != 0, "Not a valid graphics context");
    //TODO: Apply options
    
    // TODO: Refactor
    if(glThreadInfo.running == 0){
        glThreadInfo.running = 1;
        std::thread(OpenGLEntry).detach();
    }
}
