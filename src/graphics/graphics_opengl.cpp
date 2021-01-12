#include <graphics.h>
#include <types.h>
#include <thread>
#include <shaders.h>
#include <transform.h>

//TODO: Refactor these, possibly remove, dev render code only
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <fontstash.h>
#include <gl3corefontstash.h>

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
    
    glewExperimental = GL_TRUE;
    GLenum error = glGetError();
    AssertA(error == GL_NO_ERROR, "Failed to setup glfw context");
    
    GLenum glewinit = glewInit();
    AssertA(glewinit == GLEW_OK, "Failed to initialize glew");
    
    OpenGLLoadShaders();
    
    while(!glfwWindowShouldClose(glThreadInfo.window)){
        if(glThreadInfo.window){
            //TODO: Render
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
