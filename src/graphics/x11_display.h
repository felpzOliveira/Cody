/* date = January 24th 2021 8:44 am */

#ifndef X11_DISPLAY_H
#define X11_DISPLAY_H

#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>

// The XRandR extension provides mode setting and gamma control
#include <X11/extensions/Xrandr.h>
// The Xkb extension provides improved keyboard support
#include <X11/XKBlib.h>

#define OPENGL_COMPAT_PROFILE 0
#define OPENGL_CORE_PROFILE   1

#define MAKE_CONTEXT_CURRET(name) void name(void *window)
#define SWAP_BUFFERS(name) void name(void *window)
#define SWAP_INTERVAL(name) void name(int interval)
#define EXTENSION_SUPPORTED(name) int name(const char *ex)
#define GET_PROC_ADDRESS(name) void *name(const char *value)
#define DESTROY_CONTEXT(name) void name(void *window)

typedef XID GLXWindow;
typedef XID GLXDrawable;
typedef struct __GLXFBConfig* GLXFBConfig;
typedef struct __GLXcontext* GLXContext;
typedef void (*__GLXextproc)(void);
typedef MAKE_CONTEXT_CURRET(MakeContextCurrentFunc);
typedef SWAP_BUFFERS(SwapBuffersFunc);
typedef SWAP_INTERVAL(SwapIntervalFunc);
typedef EXTENSION_SUPPORTED(ExtensionSupportedFunc);
typedef GET_PROC_ADDRESS(GetProcAddrFunc);
typedef DESTROY_CONTEXT(DestroyContextFunc);

typedef struct{
    GLXContext handle;
    GLXWindow window;
    MakeContextCurrentFunc *makeCurrent;
    SwapBuffersFunc *swapBuffers;
    SwapIntervalFunc *swapInterval;
    ExtensionSupportedFunc *extensionSupported;
    GetProcAddrFunc *getProcAddress;
    DestroyContextFunc *destroy;
}GLXContextInfo;

typedef struct{
    int major;
    int minor;
    int forward;
    int profile;
    void *window;
}ContextGL;

typedef struct{
    int redBits;
    int greenBits;
    int blueBits;
    int alphaBits;
    int depthBits;
    int stencilBits;
    int accumRedBits;
    int accumGreenBits;
    int accumBlueBits;
    int accumAlphaBits;
    int auxBuffers;
    int stereo;
    int samples;
    int sRGB;
    int doublebuffer;
    int transparent;
    uintptr_t handle;
}Framebuffer;

typedef struct{
    Colormap colormap;
    Window handle;
    Window parent;
    XIC ic;
    int width, height;
    int xpos, ypos;
    int lastCursorPosX, lastCursorPosY;
    GLXContextInfo glx;
}WindowX11;

typedef struct{
    int monotonic;
    uint64_t frequency;
}Timer;

typedef struct{
    Display *display;
    int screen;
    Window root;
    XContext context;
    
    Window helperWindow;
    Cursor hiddenCursor;
    XIM im;
    
    Atom WM_DELETE_WINDOW;
    Atom NET_WM_PING;
    
    struct{
        int available;
        int detectable;
        int majorOpcode;
        int eventBase;
        int errorBase;
        int major, minor;
        unsigned int group;
    }xkb;
    
}LibHelperX11;

void test_x11_entry(WindowX11 *window);
void swap_buffers(WindowX11 *window);
void pool_events();


#endif //X11_DISPLAY_H
