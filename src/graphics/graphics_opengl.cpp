#include <graphics.h>
#include <types.h>
#include <thread>

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
    
}

void OpenGLEntry(){
    DEBUG_MSG("Initializing OpenGL graphics\n");
    int rc = glfwInit();
    Assert(rc == GLFW_TRUE, "Failed to initialize GLFW");
    
    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_RESIZABLE, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    
    glThreadInfo.window = glfwCreateWindow(glThreadInfo.width, glThreadInfo.height,
                                           "Mayhem - 0.0.1", NULL, NULL);
    Assert(glThreadInfo.window != NULL, "Failed to open window\n");
    glfwMakeContextCurrent(glThreadInfo.window);
    
    glewExperimental = GL_TRUE;
    GLenum error = glGetError();
    Assert(error == GL_NO_ERROR, "Failed to setup glfw context");
    
    GLenum glewinit = glewInit();
    Assert(glewinit == GLEW_OK, "Failed to initialize glew");
    
    while(!glfwWindowShouldClose(glThreadInfo.window)){
        if(glThreadInfo.window){
            //TODO: Render
        }
        
        glfwSwapBuffers(glThreadInfo.window);
        glfwPollEvents();
        usleep(10000);
    }
    
    DEBUG_MSG("Finalizing OpenGL graphics\n");
    glfwDestroyWindow(glThreadInfo.window);
    glfwTerminate();
    
    glThreadInfo.running = 0;
    glThreadInfo.window = nullptr;
}

void Graphics_InitializeOpenGL(Graphics_Opts *opts){
    int rc = 0;
    Assert(opts != nullptr, "Invalid OpenGL configuration");
    Assert(opts->is_opengl != 0, "Not a valid graphics context");
    //TODO: Apply options
    
    // TODO: Refactor
    if(glThreadInfo.running == 0){
        glThreadInfo.running = 1;
        std::thread(OpenGLEntry).detach();
    }
}
