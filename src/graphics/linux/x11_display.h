/* date = January 24th 2021 8:44 am */

#ifndef X11_DISPLAY_H
#define X11_DISPLAY_H

#include <stdint.h>
#include <utilities.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xcursor/Xcursor.h>

// The XRandR extension provides mode setting and gamma control
#include <X11/extensions/Xrandr.h>
// The Xkb extension provides improved keyboard support
#include <X11/XKBlib.h>

#define MOUSE_DCLICK_INTERVAL 300 // ms
#define MOUSE_DCLICK_DISTANCE 10 // pixels

#define OPENGL_COMPAT_PROFILE 0
#define OPENGL_CORE_PROFILE   1

#define MAKE_CONTEXT_CURRET(name) void name(void *window)
#define SWAP_BUFFERS(name) void name(void *window)
#define SWAP_INTERVAL(name) void name(void *window, int interval)
#define EXTENSION_SUPPORTED(name) int name(const char *ex)
#define GET_PROC_ADDRESS(name) void *name(const char *value)
#define DESTROY_CONTEXT(name) void name(void *window)

// lower 16 bits for masking
#define EventMaskScroll      (1 << 0)
#define EventMaskLeftClick   (1 << 1)
#define EventMaskRightClick  (1 << 2)
#define EventMaskPress       (1 << 3)
#define EventMaskRelease     (1 << 4)
#define EventMaskSizeChange  (1 << 5)
#define EventMaskFocusChange (1 << 6)
#define EventMaskMotion      (1 << 7)
#define EventMaskDoubleClick (1 << 8)
#define EventMaskDragAndDrop (1 << 9)

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
    void *share;
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

#define ON_SCROLL_CALLBACK(name) void name(int is_up, void *priv)
#define ON_MOUSE_LCLICK_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_RCLICK_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_PRESS_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_RELEASE_CALLBACK(name) void name(void *priv)
#define ON_MOUSE_DCLICK_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_SIZE_CHANGE_CALLBACK(name) void name(int w, int h, void *priv)
#define ON_FOCUS_CHANGE_CALLBACK(name) void name(bool in, long unsigned int id, void *priv)
#define ON_MOUSE_MOTION_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_DRAG_AND_DROP_CALLBACK(name) void name(char **paths, int count, int x, int y, void *priv)

typedef ON_SCROLL_CALLBACK(onScrollCallback);
typedef ON_MOUSE_LCLICK_CALLBACK(onMouseLClickCallback);
typedef ON_MOUSE_RCLICK_CALLBACK(onMouseRClickCallback);
typedef ON_SIZE_CHANGE_CALLBACK(onSizeChangeCallback);
typedef ON_FOCUS_CHANGE_CALLBACK(onFocusChangeCallback);
typedef ON_MOUSE_MOTION_CALLBACK(onMouseMotionCallback);
typedef ON_MOUSE_DCLICK_CALLBACK(onMouseDClickCallback);
typedef ON_MOUSE_PRESS_CALLBACK(onMousePressedCallback);
typedef ON_MOUSE_RELEASE_CALLBACK(onMouseReleasedCallback);
typedef ON_DRAG_AND_DROP_CALLBACK(onDragAndDropCallback);

template<typename Fn>
struct CallbackX11{
    Fn *fn;
    void *priv;
    uint handle;
};

typedef CallbackX11<onScrollCallback> OnScrollCallback;
typedef CallbackX11<onMouseLClickCallback> OnMouseLClickCallback;
typedef CallbackX11<onMouseRClickCallback> OnMouseRClickCallback;
typedef CallbackX11<onMousePressedCallback> OnMousePressedCallback;
typedef CallbackX11<onMouseReleasedCallback> OnMouseReleasedCallback;
typedef CallbackX11<onMouseDClickCallback> OnMouseDClickCallback;
typedef CallbackX11<onMouseMotionCallback> OnMouseMotionCallback;
typedef CallbackX11<onSizeChangeCallback> OnSizeChangeCallback;
typedef CallbackX11<onFocusChangeCallback> OnFocusChangeCallback;
typedef CallbackX11<onDragAndDropCallback> OnDragAndDropCallback;

typedef struct WindowX11{
    Colormap colormap;
    Window handle;
    Window parent;
    XIC ic;
    int width, height;
    int xpos, ypos;
    int lastCursorPosX, lastCursorPosY;
    int shouldClose;
    Time lastKeyTime;
    Time lastClickTime;
    GLXContextInfo glx;

    /*
    * Update: Let's allow windows to have multiple callbacks for the same events.
    * this helps the widget sub-system. The reason this is a List and not a std::vector
    * is because I want to calloc this and not be bound to C++.
    */
    List<OnScrollCallback> onScrollCall;
    List<OnMouseLClickCallback> onMouseLClickCall;
    List<OnMouseRClickCallback> onMouseRClickCall;
    List<OnMousePressedCallback> onMousePressedCall;
    List<OnMouseReleasedCallback> onMouseReleasedCall;
    List<OnMouseDClickCallback> onMouseDClickCall;
    List<OnMouseMotionCallback> onMouseMotionCall;
    List<OnSizeChangeCallback> onSizeChangeCall;
    List<OnFocusChangeCallback> onFocusChangeCall;
    List<OnDragAndDropCallback> onDragAndDropCall;
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
void SetNotResizable(WindowX11 *window);
void SetOpenGLVersionX11(int major, int minor);
int  WindowShouldCloseX11(WindowX11 *window);
void SetWindowShouldCloseX11(WindowX11 *window);
void SwapBuffersX11(WindowX11 *window);
void SwapIntervalX11(WindowX11 *window, int interval);
void PoolEventsX11();
void MakeContextX11(WindowX11 *window);
void WaitForEventsX11();
WindowX11 *CreateWindowX11(int width, int height, const char *title);
WindowX11 *CreateWindowX11Shared(int width, int height,
                                 const char *title, WindowX11 *share);
void DestroyWindowX11(WindowX11 *window);
void TerminateX11();
void GetLastRecordedMousePositionX11(WindowX11 *window, int *x, int *y);

double GetElapsedTime();

const char *ClipboardGetStringX11(unsigned int *size);
void ClipboardSetStringX11(char *str, unsigned int len);

bool WindowIsHandle(WindowX11 *window, long unsigned int id);

uint RegisterOnScrollCallback(WindowX11 *window, onScrollCallback *callback, void *priv);
uint RegisterOnMousePressedCallback(WindowX11 *window, onMousePressedCallback *callback, void *priv);
uint RegisterOnMouseReleasedCallback(WindowX11 *window, onMouseReleasedCallback *callback, void *priv);
uint RegisterOnMouseLeftClickCallback(WindowX11 *window, onMouseLClickCallback *callback, void *priv);
uint RegisterOnMouseRightClickCallback(WindowX11 *window, onMouseRClickCallback *callback, void *priv);
uint RegisterOnMouseDoubleClickCallback(WindowX11 *window, onMouseDClickCallback *callback, void *priv);
uint RegisterOnSizeChangeCallback(WindowX11 *window, onSizeChangeCallback *callback, void *priv);
uint RegisterOnFocusChangeCallback(WindowX11 *window, onFocusChangeCallback *callback, void *priv);
uint RegisterOnMouseMotionCallback(WindowX11 *window, onMouseMotionCallback *callback, void *priv);
uint RegisterOnDragAndDropCallback(WindowX11* window, onDragAndDropCallback* callback, void* priv);

void UnregisterCallbackByHandle(WindowX11 *window, uint handle);

#endif //X11_DISPLAY_H
