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
#define SWAP_INTERVAL(name) void name(void *window, int interval)
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

#define ON_SCROLL_CALLBACK(name) void name(int is_up)
#define ON_MOUSE_CLICK_CALLBACK(name) void name(int x, int y)
#define ON_SIZE_CHANGE_CALLBACK(name) void name(int w, int h)
#define ON_FOCUS_CHANGE_CALLBACK(name) void name()
#define ON_MOUSE_MOTION_CALLBACK(name) void name(int x, int y)

typedef ON_SCROLL_CALLBACK(onScrollCallback);
typedef ON_MOUSE_CLICK_CALLBACK(onMouseClickCallback);
typedef ON_SIZE_CHANGE_CALLBACK(onSizeChangeCallback);
typedef ON_FOCUS_CHANGE_CALLBACK(onFocusChangeCallback);
typedef ON_MOUSE_MOTION_CALLBACK(onMouseMotionCallback);

typedef struct{
    Colormap colormap;
    Window handle;
    Window parent;
    XIC ic;
    int width, height;
    int xpos, ypos;
    int lastCursorPosX, lastCursorPosY;
    int shouldClose;
    Time lastKeyTime;
    GLXContextInfo glx;
    
    onScrollCallback *onScrollCall;
    onMouseClickCallback *onMouseClickCall;
    onMouseMotionCallback *onMouseMotionCall;
    onSizeChangeCallback *onSizeChangeCall;
    onFocusChangeCallback *onFocusChangeCall;
}WindowX11;

typedef struct{
    int monotonic;
    uint64_t frequency;
    uint64_t offset;
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
    Atom TARGETS;
    Atom MULTIPLE;
    Atom INCR;
    Atom CLIPBOARD;
    Atom PRIMARY;
    Atom CLIPBOARD_MANAGER;
    Atom SAVE_TARGETS;
    Atom ATOM_PAIR;
    Atom NULL_;
    Atom UTF8_STRING;
    Atom COMPOUND_STRING;
    Atom CODY_SELECTION;
    
    char *primarySelectionString;
    char *clipboardString;
    unsigned int primarySelectionStringSize;
    unsigned int clipboardStringSize;
    
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

void InitializeX11();
void SetSamplesX11(int samples);
void SetOpenGLVersionX11(int major, int minor);
int  WindowShouldCloseX11(WindowX11 *window);
void SwapBuffersX11(WindowX11 *window);
void SwapIntervalX11(WindowX11 *window, int interval);
void PoolEventsX11();
void WaitForEventsX11();
WindowX11 *CreateWindowX11(int width, int height, const char *title);
void DestroyWindowX11(WindowX11 *window);
void TerminateX11();
void GetLastRecordedMousePositionX11(WindowX11 *window, int *x, int *y);

double GetElapsedTime();

const char *ClipboardGetStringX11(unsigned int *size);
void ClipboardSetStringX11(char *str, unsigned int len);

void RegisterOnScrollCallback(WindowX11 *window, onScrollCallback *callback);
void RegisterOnMouseClickCallback(WindowX11 *window, onMouseClickCallback *callback);
void RegisterOnSizeChangeCallback(WindowX11 *window, onSizeChangeCallback *callback);
void RegisterOnFocusChangeCallback(WindowX11 *window, onFocusChangeCallback *callback);
void RegisterOnMouseMotionCallback(WindowX11 *window, onMouseMotionCallback *callback);

#endif //X11_DISPLAY_H
