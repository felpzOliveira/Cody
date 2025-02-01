#include <x11_display.h>
#include <X11/Xresource.h>
#include <types.h>
#include <utilities.h>
#include <time.h>
#include <string.h>
#include <glx_helper.h>
#include <unistd.h>
#include <sys/time.h>
#include <x11_keyboard.h>
#include <climits>
#include <vector>

// NOTE: Remove if compiling this for other projects or implement a PNG loading function.
#define INCLUDE_IMAGE_UTILS

#if defined(INCLUDE_IMAGE_UTILS)
    uint8 *ImageUtils_LoadPixels(uint8 *data, uint len, int *width, int *height, int *channels);
#endif

LibHelperX11 x11Helper;
Timer timer;
Framebuffer x11Framebuffer;
ContextGL x11GLContext;

#define SkipEventNoWindow(window) do{ if(!window) return; }while(0)

void ProcessEventX11(XEvent *event);

#define CallbackComparator(hnd, field, wnd, type, val) do{\
    bool __cc_ok = true;\
    List_ForAllItemsInterruptable(&(wnd)->field, [&](type *__cc_item) -> int{\
        if(hnd == __cc_item->handle) __cc_ok = false;\
        return __cc_ok ? 1 : 0;\
    });\
    val = __cc_ok;\
}while(0)

#define RegisterCallback(wnd, cb, priv, field, type, mask) do{\
    uint __rc_handle = 0;\
    bool __rc_generated = false;\
    do{\
        __rc_handle = Bad_RNG16() | (mask << 8);\
        CallbackComparator(__rc_handle, field, wnd, type, __rc_generated);\
    }while(!__rc_generated);\
    type __rc_cc = {\
        .fn = cb,\
        .priv = priv,\
        .handle = __rc_handle,\
    };\
    List_Push(&(wnd)->field, &__rc_cc);\
    return __rc_handle;\
}while(0)

#define UnregisterCallback(wnd, hnd, field, type) do{\
    auto finder = [&](type *item) -> int{\
        return (item->handle == hnd ? 1 : 0);\
    };\
    List_Erase(&(wnd)->field, finder);\
}while(0)

size_t encodeUTF8(char* s, unsigned int ch){
    size_t count = 0;

    if(ch < 0x80)
        s[count++] = (char) ch;
    else if (ch < 0x800){
        s[count++] = (ch >> 6) | 0xc0;
        s[count++] = (ch & 0x3f) | 0x80;
    }
    else if(ch < 0x10000){
        s[count++] = (ch >> 12) | 0xe0;
        s[count++] = ((ch >> 6) & 0x3f) | 0x80;
        s[count++] = (ch & 0x3f) | 0x80;
    }
    else if(ch < 0x110000){
        s[count++] = (ch >> 18) | 0xf0;
        s[count++] = ((ch >> 12) & 0x3f) | 0x80;
        s[count++] = ((ch >> 6) & 0x3f) | 0x80;
        s[count++] = (ch & 0x3f) | 0x80;
    }

    return count;
}

static char* convertLatin1toUTF8(const char* source){
    size_t size = 1;
    const char* sp;

    for(sp = source;  *sp;  sp++)
        size += (*sp & 0x80) ? 2 : 1;

    char* target = (char *)calloc(size, 1);
    char* tp = target;

    for(sp = source;  *sp;  sp++)
        tp += encodeUTF8(tp, *sp);

    return target;
}

uint64_t GetTimerValue(){
    if(timer.monotonic){
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t) ts.tv_sec * (uint64_t) 1000000000 + (uint64_t) ts.tv_nsec;
    }
    else{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t) tv.tv_sec * (uint64_t) 1000000 + (uint64_t) tv.tv_usec;
    }
}

void InitializeTimer(){
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0){
        timer.monotonic = 1;
        timer.frequency = 1000000000;
    }else{
        timer.monotonic = 0;
        timer.frequency = 1000000;
    }

    timer.offset = GetTimerValue();
}

uint64_t GetTimerFrequency(){
    return timer.frequency;
}

double GetElapsedTime(){
    return (double)(GetTimerValue() - timer.offset) / GetTimerFrequency();
}

int waitForEvent(double* timeout){
    fd_set fds;
    const int fd = ConnectionNumber(x11Helper.display);
    int count = fd + 1;

    for(;;){
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if(timeout){
            const long seconds = (long) *timeout;
            const long microseconds = (long) ((*timeout - seconds) * 1e6);
            struct timeval tv = { seconds, microseconds };
            const uint64_t base = GetTimerValue();

            const int result = select(count, &fds, NULL, NULL, &tv);
            const int error = errno;

            *timeout -= (GetTimerValue() - base) / (double) GetTimerFrequency();

            if(result > 0)
                return 1;
            if((result == -1 && error == EINTR) || *timeout <= 0.0)
                return 0;
        }else if (select(count, &fds, NULL, NULL, NULL) != -1 || errno != EINTR)
            return 1;
    }
}

void InitializeFramebuffer(Framebuffer *framebuffer){
    memset(framebuffer, 0x0, sizeof(Framebuffer));
    framebuffer->redBits      = 8;
    framebuffer->greenBits    = 8;
    framebuffer->blueBits     = 8;
    framebuffer->alphaBits    = 8;
    framebuffer->depthBits    = 24;
    framebuffer->stencilBits  = 8;
    framebuffer->doublebuffer = 1;
}

Window CreateHelperWindow(LibHelperX11 *x11){
    XSetWindowAttributes wa;
    wa.event_mask = PropertyChangeMask;
    return XCreateWindow(x11->display, x11->root, 0, 0, 1, 1, 0, 0,
                         InputOnly, DefaultVisual(x11->display, x11->screen),
                         CWEventMask, &wa);
}

void XkbSetup(LibHelperX11 *x11){
    x11->xkb.major = 1;
    x11->xkb.minor = 0;
    x11->xkb.available = XkbQueryExtension(x11->display, &x11->xkb.majorOpcode,
                                           &x11->xkb.eventBase, &x11->xkb.errorBase,
                                           &x11->xkb.major, &x11->xkb.minor);

    if(x11->xkb.available){
        XkbStateRec state;
        int supported;
        if(XkbSetDetectableAutoRepeat(x11->display, 1, &supported)){
            if(supported){
                x11->xkb.detectable = 1;
            }
        }

        x11->xkb.group = 0;
        if(XkbGetState(x11->display, XkbUseCoreKbd, &state) == Success){
            XkbSelectEventDetails(x11->display, XkbUseCoreKbd, XkbStateNotify,
                                  XkbAllStateComponentsMask, XkbGroupStateMask);
            x11->xkb.group = (unsigned int)state.group;
        }
    }
}

int XIMHasInputStyles(LibHelperX11 *x11){
    XIMStyles* styles = NULL;
    int found_styles = 0;

    if(XGetIMValues(x11Helper.im, XNQueryInputStyle, &styles, NULL) != NULL){
        goto __ret;
    }

    for(uint i = 0; i < styles->count_styles; i++){
        if(styles->supported_styles[i] == (XIMPreeditNothing | XIMStatusNothing)){
            found_styles = 1;
            break;
        }
    }

    XFree(styles);

__ret:
    return found_styles;
}

void InitializeX11(){
    XInitThreads();
    XrmInitialize();

    x11Helper.display = XOpenDisplay(NULL);

    AssertErr(x11Helper.display != NULL, "Failed to connect to X");

    x11Helper.screen  = DefaultScreen(x11Helper.display);
    x11Helper.root    = RootWindow(x11Helper.display, x11Helper.screen);
    x11Helper.context = XUniqueContext();

    XkbSetup(&x11Helper);
    x11Helper.helperWindow = CreateHelperWindow(&x11Helper);
    x11Helper.hiddenCursor = None; //TODO

    AssertErr(setlocale(LC_ALL, "") != NULL, "Failed to set locale");
    AssertErr(XSupportsLocale(), "X does not supporte locale");
    AssertErr(XSetLocaleModifiers("@im=none") != NULL, "Failed to set locale modifiers");

    x11Helper.im = XOpenIM(x11Helper.display, 0, NULL, NULL);
    AssertErr(x11Helper.im != NULL, "Failed to open input method");

    AssertErr(XIMHasInputStyles(&x11Helper) != 0, "No input styles found");

    InitializeTimer();
    InitializeFramebuffer(&x11Framebuffer);

    //TODO: Expose?
    x11GLContext.forward = 1;
    x11GLContext.profile = OPENGL_CORE_PROFILE;

    x11Helper.WM_DELETE_WINDOW = XInternAtom(x11Helper.display, "WM_DELETE_WINDOW", False);
    x11Helper.NET_WM_PING = XInternAtom(x11Helper.display, "_NET_WM_PING", False);
    x11Helper.NET_WM_ICON = XInternAtom(x11Helper.display, "_NET_WM_ICON", False);

    x11Helper.TARGETS = XInternAtom(x11Helper.display, "TARGETS", False);
    x11Helper.MULTIPLE = XInternAtom(x11Helper.display, "MULTIPLE", False);
    x11Helper.PRIMARY = XInternAtom(x11Helper.display, "PRIMARY", False);
    x11Helper.INCR = XInternAtom(x11Helper.display, "INCR", False);
    x11Helper.CLIPBOARD = XInternAtom(x11Helper.display, "CLIPBOARD", False);

    x11Helper.CLIPBOARD_MANAGER =
        XInternAtom(x11Helper.display, "CLIPBOARD_MANAGER", False);
    x11Helper.SAVE_TARGETS =
        XInternAtom(x11Helper.display, "SAVE_TARGETS", False);

    x11Helper.NULL_ = XInternAtom(x11Helper.display, "NULL", False);
    x11Helper.UTF8_STRING = XInternAtom(x11Helper.display, "UTF8_STRING", False);
    x11Helper.ATOM_PAIR = XInternAtom(x11Helper.display, "ATOM_PAIR", False);
    x11Helper.CODY_SELECTION =
        XInternAtom(x11Helper.display, "CODY_SELECTION", False);
}

int WindowShouldCloseX11(WindowX11 *window){
    return window->shouldClose;
}

void SetWindowShouldCloseX11(WindowX11 *window){
    window->shouldClose = 1;
}

int IsWindowVisibleX11(WindowX11 *window, LibHelperX11 *x11){
    XWindowAttributes wa;
    XGetWindowAttributes(x11->display, window->handle, &wa);
    return wa.map_state == IsViewable;
}

int waitForVisibilityNotify(WindowX11* window, LibHelperX11 *x11){
    XEvent dummy;
    double timeout = 0.1;
    while (!XCheckTypedWindowEvent(x11->display,
                                   window->handle,
                                   VisibilityNotify,
                                   &dummy))
    {
        if(!waitForEvent(&timeout)){
            return 0;
        }
    }

    return 1;
}

void MakeContextX11(WindowX11 *window){
    MakeContextCurrentGLX((void *)window);
}

void _CreateWindowX11(int width, int height, const char *title,
                      Framebuffer *fb, ContextGL *context, LibHelperX11 *x11,
                      WindowX11 *window)
{
    Visual *visual;
    int depth = 0;
    AssertErr(InitGLX(x11) == 1, "Failed to initialize GLX");
    ChooseVisualGLX(fb, x11, &visual, &depth);

    window->colormap = XCreateColormap(x11->display, x11->root, visual, AllocNone);
    XSetWindowAttributes wa = { 0 };

    wa.colormap = window->colormap;
#if 1
    wa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
        ExposureMask | FocusChangeMask | VisibilityChangeMask |
        EnterWindowMask | LeaveWindowMask | PropertyChangeMask;
#else
    wa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask |
        ExposureMask | FocusChangeMask | VisibilityChangeMask |
        PropertyChangeMask;
#endif
    window->parent = x11->root;
    window->handle = XCreateWindow(x11->display, x11->root, 0, 0,
                                   width, height, 0, depth, InputOutput,
                                   visual, CWBorderPixel | CWColormap | CWEventMask,
                                   &wa);
    AssertA(window->handle, "Failed to create window");

    XSaveContext(x11->display, window->handle, x11->context, (XPointer) window);

    Xutf8SetWMProperties(x11->display, window->handle, title, title,
                         NULL, 0, NULL, NULL, NULL);

    window->ic = XCreateIC(x11->im, XNInputStyle,
                           XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
                           window->handle, XNFocusWindow,
                           window->handle, NULL);

    Atom protocols[] = {
        x11->WM_DELETE_WINDOW,
        x11->NET_WM_PING
    };

    XSetWMProtocols(x11->display, window->handle,
                    protocols, sizeof(protocols) / sizeof(Atom));

    unsigned long filter = 0;
    if (XGetICValues(window->ic, XNFilterEvents, &filter, NULL) == NULL)
        XSelectInput(x11->display, window->handle, wa.event_mask | filter);

    XSetICFocus(window->ic);

    window->width  = width;
    window->height = height;
    context->window = (WindowX11 *)window;

    (void)CreateContextGLX(context, fb, x11);

    MakeContextCurrentGLX((void *)window);

    XMapWindow(x11->display, window->handle);
    waitForVisibilityNotify(window, x11);

    XClassHint *xHint = XAllocClassHint();
    if(xHint){
        xHint->res_name = xHint->res_class = (char *)"Cody"; //TODO: app name
        XSetClassHint(x11->display, window->handle, xHint);
        XFree(xHint);
    }

    window->shouldClose = 0;

    XFlush(x11->display);
}

void SetSamplesX11(int samples){
    x11Framebuffer.samples = Max(0, samples);
}

void SetNotResizable(WindowX11 *window){
    XSizeHints* hints = XAllocSizeHints();
    hints->flags |= PWinGravity;
    hints->win_gravity = StaticGravity;
    hints->flags |= (PMinSize | PMaxSize);
    hints->min_width  = hints->max_width  = window->width;
    hints->min_height = hints->max_height = window->height;
    XSetWMNormalHints(x11Helper.display, window->handle, hints);
    XFree(hints);
}


void SetOpenGLVersionX11(int major, int minor){
    x11GLContext.major = major;
    x11GLContext.minor = minor;
}

uint RegisterOnScrollCallback(WindowX11 *window, onScrollCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onScrollCall,
                     OnScrollCallback, EventMaskScroll);
}

uint RegisterOnMouseLeftClickCallback(WindowX11 *window, onMouseLClickCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMouseLClickCall,
                     OnMouseLClickCallback, EventMaskLeftClick);
}

uint RegisterOnMouseRightClickCallback(WindowX11 *window, onMouseRClickCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMouseRClickCall,
                     OnMouseRClickCallback, EventMaskRightClick);
}

uint RegisterOnMousePressedCallback(WindowX11 *window, onMousePressedCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMousePressedCall,
                     OnMousePressedCallback, EventMaskPress);
}

uint RegisterOnMouseReleasedCallback(WindowX11 *window, onMouseReleasedCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMouseReleasedCall,
                     OnMouseReleasedCallback, EventMaskRelease);
}

uint RegisterOnSizeChangeCallback(WindowX11 *window, onSizeChangeCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onSizeChangeCall,
                     OnSizeChangeCallback, EventMaskSizeChange);
}

uint RegisterOnFocusChangeCallback(WindowX11 *window, onFocusChangeCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onFocusChangeCall,
                     OnFocusChangeCallback, EventMaskFocusChange);
}

uint RegisterOnMouseMotionCallback(WindowX11 *window, onMouseMotionCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMouseMotionCall,
                     OnMouseMotionCallback, EventMaskMotion);
}

uint RegisterOnMouseDoubleClickCallback(WindowX11 *window, onMouseDClickCallback *callback, void *priv){
    RegisterCallback(window, callback, priv, onMouseDClickCall,
                     OnMouseDClickCallback, EventMaskDoubleClick);
}

uint RegisterOnDragAndDropCallback(WindowX11* window, onDragAndDropCallback* callback, void* priv){
    RegisterCallback(window, callback, priv, onDragAndDropCall, OnDragAndDropCallback, EventMaskDragAndDrop);
}

void UnregisterCallbackByHandle(WindowX11 *wnd, uint hnd){
    uint bits = hnd >> 8;
    switch(bits){
        case EventMaskScroll:{
            UnregisterCallback(wnd, hnd, onScrollCall, OnScrollCallback);
        } break;
        case EventMaskLeftClick:{
            UnregisterCallback(wnd, hnd, onMouseLClickCall, OnMouseLClickCallback);
        } break;
        case EventMaskRightClick:{
            UnregisterCallback(wnd, hnd, onMouseRClickCall, OnMouseRClickCallback);
        } break;
        case EventMaskPress:{
            UnregisterCallback(wnd, hnd, onMousePressedCall, OnMousePressedCallback);
        } break;
        case EventMaskRelease:{
            UnregisterCallback(wnd, hnd, onMouseReleasedCall, OnMouseReleasedCallback);
        } break;
        case EventMaskSizeChange:{
            UnregisterCallback(wnd, hnd, onSizeChangeCall, OnSizeChangeCallback);
        } break;
        case EventMaskFocusChange:{
            UnregisterCallback(wnd, hnd, onFocusChangeCall, OnFocusChangeCallback);
        } break;
        case EventMaskMotion:{
            UnregisterCallback(wnd, hnd, onMouseMotionCall, OnMouseMotionCallback);
        } break;
        case EventMaskDoubleClick:{
            UnregisterCallback(wnd, hnd, onMouseDClickCall, OnMouseDClickCallback);
        } break;
        case EventMaskDragAndDrop: {
            UnregisterCallback(wnd, hnd, onDragAndDropCall, OnDragAndDropCallback);
        } break;
        default:{
            printf("Unkown mask value %u\n", bits);
        }
    }
}

static void InitWindowCallbackListX11(WindowX11 *window){
    List_Init(&window->onScrollCall);
    List_Init(&window->onMouseLClickCall);
    List_Init(&window->onMouseRClickCall);
    List_Init(&window->onMousePressedCall);
    List_Init(&window->onMouseDClickCall);
    List_Init(&window->onMouseMotionCall);
    List_Init(&window->onSizeChangeCall);
    List_Init(&window->onFocusChangeCall);
    List_Init(&window->onDragAndDropCall);
}

static void ClearWindowCallbackListX11(WindowX11 *window){
    List_Clear(&window->onScrollCall);
    List_Clear(&window->onMouseLClickCall);
    List_Clear(&window->onMouseRClickCall);
    List_Clear(&window->onMousePressedCall);
    List_Clear(&window->onMouseDClickCall);
    List_Clear(&window->onMouseMotionCall);
    List_Clear(&window->onSizeChangeCall);
    List_Clear(&window->onFocusChangeCall);
    List_Clear(&window->onDragAndDropCall);
}

WindowX11 *CreateWindowX11Shared(int width, int height,
                                 const char *title, WindowX11 *share)
{
    WindowX11 *window = AllocatorGetN(WindowX11, 1);
    x11GLContext.share = (void *)share;
    _CreateWindowX11(width, height, title, &x11Framebuffer,
                     &x11GLContext, &x11Helper, window);
    InitWindowCallbackListX11(window);
    return window;
}

WindowX11 *CreateWindowX11(int width, int height, const char *title){
    WindowX11 *window = AllocatorGetN(WindowX11, 1);
    _CreateWindowX11(width, height, title, &x11Framebuffer,
                     &x11GLContext, &x11Helper, window);
    InitWindowCallbackListX11(window);
    return window;
}

void TerminateX11(){
    XCloseIM(x11Helper.im);
    XCloseDisplay(x11Helper.display);
    TerminateGLX();
}

void DestroyWindowX11(WindowX11 *window){
    if(window){
        XDestroyIC(window->ic);
        DestroyContextGLX((void *)window);

        XDeleteContext(x11Helper.display, window->handle, x11Helper.context);
        XUnmapWindow(x11Helper.display, window->handle);
        XDestroyWindow(x11Helper.display, window->handle);

        XFreeColormap(x11Helper.display, window->colormap);
        XFlush(x11Helper.display);

        ClearWindowCallbackListX11(window);

        free(window);
    }
}

void SwapIntervalX11(WindowX11 *window, int interval){
    SwapIntervalGLX((void *)window, interval);
}

void SwapBuffersX11(WindowX11 *window){
    SwapBufferGLX((void *)window);
}

static int isSelPropNewValueNotify(Display* display, XEvent* event, XPointer pointer){
    XEvent* notification = (XEvent*) pointer;
    return event->type == PropertyNotify &&
        event->xproperty.state == PropertyNewValue &&
        event->xproperty.window == notification->xselection.requestor &&
        event->xproperty.atom == notification->xselection.property;
}

unsigned long GetWindowPropertyX11(Window window, Atom property, Atom type,
                                   unsigned char** value)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount, bytesAfter;

    XGetWindowProperty(x11Helper.display,
                       window,
                       property,
                       0,
                       LONG_MAX,
                       False,
                       type,
                       &actualType,
                       &actualFormat,
                       &itemCount,
                       &bytesAfter,
                       value);

    return itemCount;
}

static Atom WriteTargetToProperty(const XSelectionRequestEvent* request){
    int i;
    char* selectionString = NULL;
    const Atom formats[] = { x11Helper.UTF8_STRING, XA_STRING };
    const int formatCount = sizeof(formats) / sizeof(formats[0]);
    unsigned int selectionSize = 0;

    if(request->selection == x11Helper.PRIMARY){
        selectionSize   = x11Helper.primarySelectionStringSize;
        selectionString = x11Helper.primarySelectionString;
    }else{
        selectionSize   = x11Helper.clipboardStringSize;
        selectionString = x11Helper.clipboardString;
    }

    if(request->property == None){
        // The requester is a legacy client (ICCCM section 2.2)
        // We don't support legacy clients, so fail here
        return None;
    }

    if(request->target == x11Helper.TARGETS){
        // The list of supported targets was requested

        const Atom targets[] = { x11Helper.TARGETS,
            x11Helper.MULTIPLE,
            x11Helper.UTF8_STRING,
            XA_STRING };

        XChangeProperty(x11Helper.display,
                        request->requestor,
                        request->property,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) targets,
                        sizeof(targets) / sizeof(targets[0]));

        return request->property;
    }

    if(request->target == x11Helper.MULTIPLE){
        // Multiple conversions were requested

        Atom* targets;
        unsigned long i, count;

        count = GetWindowPropertyX11(request->requestor,
                                     request->property,
                                     x11Helper.ATOM_PAIR,
                                     (unsigned char**) &targets);

        for (i = 0; i < count; i += 2){
            int j;

            for(j = 0; j < formatCount; j++){
                if (targets[i] == formats[j])
                    break;
            }

            if(j < formatCount){
                XChangeProperty(x11Helper.display,
                                request->requestor,
                                targets[i + 1],
                                targets[i],
                                8,
                                PropModeReplace,
                                (unsigned char *) selectionString,
                                selectionSize);
            }
            else
                targets[i + 1] = None;
        }

        XChangeProperty(x11Helper.display,
                        request->requestor,
                        request->property,
                        x11Helper.ATOM_PAIR,
                        32,
                        PropModeReplace,
                        (unsigned char*) targets,
                        count);

        XFree(targets);

        return request->property;
    }

    if(request->target == x11Helper.SAVE_TARGETS){
        // The request is a check whether we support SAVE_TARGETS
        // It should be handled as a no-op side effect target

        XChangeProperty(x11Helper.display,
                        request->requestor,
                        request->property,
                        x11Helper.NULL_,
                        32,
                        PropModeReplace,
                        NULL,
                        0);

        return request->property;
    }

    // Conversion to a data target was requested

    for(i = 0; i < formatCount; i++){
        if(request->target == formats[i]){
            // The requested target is one we support

            XChangeProperty(x11Helper.display,
                            request->requestor,
                            request->property,
                            request->target,
                            8,
                            PropModeReplace,
                            (unsigned char *) selectionString,
                            selectionSize);

            return request->property;
        }
    }

    // The requested target is not supported

    return None;
}

const char* GetSelectionString(Atom selection, unsigned int *outlen){
    char** selectionString = NULL;
    const Atom targets[] = { x11Helper.UTF8_STRING, XA_STRING };
    const size_t targetCount = sizeof(targets) / sizeof(targets[0]);
    unsigned int *selectionSize = NULL;

    if(selection == x11Helper.PRIMARY){
        selectionSize   = &x11Helper.primarySelectionStringSize;
        selectionString = &x11Helper.primarySelectionString;
    }else{
        selectionSize   = &x11Helper.clipboardStringSize;
        selectionString = &x11Helper.clipboardString;
    }

    *outlen = *selectionSize;

    if(XGetSelectionOwner(x11Helper.display, selection) ==
       x11Helper.helperWindow)
    {
        // Instead of doing a large number of X round-trips just to put this
        // string into a window property and then read it back, just return it
        return *selectionString;
    }

    free(*selectionString);
    *selectionString = NULL;

    for(size_t i = 0;  i < targetCount;  i++){
        char* data;
        Atom actualType;
        int actualFormat;
        unsigned long itemCount, bytesAfter;
        XEvent notification, dummy;

        XConvertSelection(x11Helper.display,
                          selection,
                          targets[i],
                          x11Helper.CODY_SELECTION,
                          x11Helper.helperWindow,
                          CurrentTime);

        while(!XCheckTypedWindowEvent(x11Helper.display,
                                      x11Helper.helperWindow,
                                      SelectionNotify,
                                      &notification))
        {
            waitForEvent(NULL);
        }

        if(notification.xselection.property == None)
            continue;

        XCheckIfEvent(x11Helper.display,
                      &dummy,
                      isSelPropNewValueNotify,
                      (XPointer) &notification);

        XGetWindowProperty(x11Helper.display,
                           notification.xselection.requestor,
                           notification.xselection.property,
                           0, LONG_MAX, True, AnyPropertyType,
                           &actualType, &actualFormat,
                           &itemCount, &bytesAfter,
                           (unsigned char**) &data);

        if(actualType == x11Helper.INCR){
            size_t size = 1;
            char* string = NULL;

            for(;;){
                while(!XCheckIfEvent(x11Helper.display,
                                     &dummy,
                                     isSelPropNewValueNotify,
                                     (XPointer) &notification))
                {
                    waitForEvent(NULL);
                }

                XFree(data);
                XGetWindowProperty(x11Helper.display,
                                   notification.xselection.requestor,
                                   notification.xselection.property,
                                   0, LONG_MAX, True, AnyPropertyType,
                                   &actualType, &actualFormat,
                                   &itemCount, &bytesAfter,
                                   (unsigned char**) &data);

                if(itemCount){
                    size += itemCount;
                    string = (char *)realloc(string, size);
                    string[size - itemCount - 1] = '\0';
                    strcat(string, data);
                }

                if(!itemCount){
                    if (targets[i] == XA_STRING){
                        *selectionString = convertLatin1toUTF8(string);
                        free(string);
                    }
                    else
                        *selectionString = string;
                    break;
                }
            }
        }
        else if(actualType == targets[i]){
            if(targets[i] == XA_STRING)
                *selectionString = convertLatin1toUTF8(data);
            else
                *selectionString = strdup(data);
        }

        XFree(data);

        if(*selectionString)
            break;
    }

    if(!*selectionString){
        printf("Failed to copy content\n");
    }

    *selectionSize = strlen(*selectionString);
    *outlen = *selectionSize;
    return *selectionString;

}

const char *ClipboardGetStringX11(unsigned int *size){
    const char *str = GetSelectionString(x11Helper.CLIPBOARD, size);
    return str;
}

void ClipboardSetStringX11(char *str, unsigned int len){
    free(x11Helper.clipboardString);
    x11Helper.clipboardString = str;
    x11Helper.clipboardStringSize = len;
    XSetSelectionOwner(x11Helper.display, x11Helper.CLIPBOARD,
                       x11Helper.helperWindow, CurrentTime);

    if(XGetSelectionOwner(x11Helper.display, x11Helper.CLIPBOARD) !=
       x11Helper.helperWindow)
    {
        printf("Failed to get CLIPBOARD\n");
    }
}

void GetLastRecordedMousePositionX11(WindowX11 *window, int *x, int *y){
    if(window){
        if(x) *x = window->lastCursorPosX;
        if(y) *y = window->lastCursorPosY;
    }
}

void SetWindowIconX11(WindowX11 *window, unsigned char *png, unsigned int pngLen){
    uint len = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned long *iconMem = nullptr;
    unsigned long *ref = nullptr;
    int longLen = 0;

    uint8 *memory = ImageUtils_LoadPixels(png, pngLen, &width, &height, &channels);

    if(!memory)
        goto __ret;

    longLen = width * height + 2;
    iconMem = (unsigned long *)calloc(longLen, sizeof(unsigned long));

    if(!iconMem)
        goto __ret;

    ref = iconMem;

    *ref++ = width;
    *ref++ = height;

    // TODO: channels != 4?
    for(int j = 0;  j < width * height;  j++){
        *ref++ = (((unsigned long) memory[j * 4 + 0]) << 16) |
                 (((unsigned long) memory[j * 4 + 1]) <<  8) |
                 (((unsigned long) memory[j * 4 + 2]) <<  0) |
                 (((unsigned long) memory[j * 4 + 3]) << 24);
    }

    XChangeProperty(x11Helper.display, window->handle,
                    x11Helper.NET_WM_ICON, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *) iconMem, longLen);

    free(iconMem);

    XFlush(x11Helper.display);
__ret:
    if(memory)
        free(memory);
}

void WaitForEventsX11(){
    while(!XPending(x11Helper.display)){
        waitForEvent(NULL);
    }

    PoolEventsX11();
}

void PoolEventsX11(){
    XPending(x11Helper.display);

    while(XQLength(x11Helper.display)){
        XEvent event;
        XNextEvent(x11Helper.display, &event);
        ProcessEventX11(&event);
    }

    //TODO: Figure out this cursor thingy in glfw
    XFlush(x11Helper.display);
}

void ProcessEventReparentX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    window->parent = event->xreparent.parent;
}

void ProcessEventKeyPressX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    int count = 0;
    int code = event->xkey.keycode;
    KeySym keysym = 0;
    char buf[20];
    Status status = 0;

    // Somehow we getting duplicated events lets filter them out
    if(!(window->lastKeyTime < event->xkey.time)) return;

    int mappedKey = TranslateKey(code);
    // TODO: How do we input text in other encodings?
    count = Xutf8LookupString(window->ic, (XKeyPressedEvent*)event, buf,
                              20, &keysym, &status);

    if(status == XBufferOverflow){
        printf("BufferOverflow\n");
        AssertA(0, "Weird UTF8 character input");
    }

    KeyboardEvent((Key)mappedKey, KEYBOARD_EVENT_PRESS,
                  buf, count, code, (void *)window, 1);

    window->lastKeyTime = event->xkey.time;
}

void ProcessEventKeyReleaseX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    int code = event->xkey.keycode;
    int mappedKey = TranslateKey(code);
    KeyboardEvent((Key)mappedKey, KEYBOARD_EVENT_RELEASE,
                  NULL, 0, code, (void *)window, 1);
}

void ProcessEventButtonPressX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    int is_scroll = 0;
    int is_scroll_up = 0;
    switch(event->xbutton.button){
        case Button4:{
            is_scroll_up = 1;
            is_scroll = 1;
        } break;
        case Button5:{
            is_scroll = 1;
        } break;
        case Button1:{
            List_ForAllItems(&window->onMousePressedCall, [=](OnMousePressedCallback *sc){
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });
        } break;
        default:{ }
    }

    if(is_scroll){
        List_ForAllItems(&window->onScrollCall, [=](OnScrollCallback *sc){
            sc->fn(is_scroll_up, sc->priv);
        });
    }
}

void ProcessEventButtonReleaseX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    switch(event->xbutton.button){
        case Button1:{
            List_ForAllItems(&window->onMouseReleasedCall, [=](OnMouseReleasedCallback *sc){
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });

            List_ForAllItems(&window->onMouseLClickCall, [=](OnMouseLClickCallback *sc){
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });

            // I really hate that I have to do this, because all I wanted
            // was to be happy. But aparently linux does not want me
            // to be happy.
            double dif = event->xbutton.time - window->lastClickTime;
            vec2f vold((Float)window->xpos, (Float)window->ypos);
            vec2f vnew((Float)window->lastCursorPosX, (Float)window->lastCursorPosY);
            Float dist = (vold - vnew).Length();

            if(dif < MOUSE_DCLICK_INTERVAL && dist < MOUSE_DCLICK_DISTANCE){
                List_ForAllItems(&window->onMouseDClickCall, [=](OnMouseDClickCallback *sc){
                    sc->fn(window->lastCursorPosX, window->lastCursorPosY,sc->priv);
                });
            }
            window->lastClickTime = event->xbutton.time;
            window->xpos = window->lastCursorPosX;
            window->ypos = window->lastCursorPosY;
        } break;
        case Button3: {
            List_ForAllItems(&window->onMouseRClickCall, [=](OnMouseRClickCallback *sc){
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });
        } break;
        default:{ }
    }
}

void ProcessEventEnterX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Enter Notify\n");
}

void ProcessEventLeaveX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Leave Notify\n");
}

void ProcessEventMotionX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    int x = event->xmotion.x;
    int y = event->xmotion.y;
    window->lastCursorPosX = x;
    window->lastCursorPosY = y;
    List_ForAllItems(&window->onMouseMotionCall, [=](OnMouseMotionCallback *sc){
        sc->fn(x, y, sc->priv);
    });
}

void ProcessEventConfigureX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    if(event->xconfigure.width != window->width ||
       event->xconfigure.height != window->height)
    {
        window->width = event->xconfigure.width;
        window->height = event->xconfigure.height;

        List_ForAllItems(&window->onSizeChangeCall, [=](OnSizeChangeCallback *sc){
            sc->fn(window->width, window->height, sc->priv);
        });
    }
}

void ProcessEventClientMessageX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //TODO: Protocols
    SkipEventNoWindow(window);
    const Atom protocol = event->xclient.data.l[0];
    if(protocol == x11Helper.WM_DELETE_WINDOW){
        window->shouldClose = 1;
    }else if(protocol == x11Helper.NET_WM_PING){
        XEvent reply = *event;
        reply.xclient.window = x11Helper.root;
        XSendEvent(x11Helper.display, x11Helper.root, False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &reply);
    }
}

void ProcessEventSelectionX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    printf("Selection Notify\n");
}

void ProcessEventSelectionClearX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
#if 0
    if(event->xselectionclear.selection == x11Helper.PRIMARY){
        free(x11Helper.primarySelectionString);
        x11Helper.primarySelectionString = NULL;
    }else{
        free(x11Helper.clipboardString);
        x11Helper.clipboardString = NULL;
    }
#endif
}

void ProcessEventSelectionRequestX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    const XSelectionRequestEvent* request = &event->xselectionrequest;
    XEvent reply = { SelectionNotify };
    reply.xselection.property = WriteTargetToProperty(request);
    reply.xselection.display = request->display;
    reply.xselection.requestor = request->requestor;
    reply.xselection.selection = request->selection;
    reply.xselection.target = request->target;
    reply.xselection.time = request->time;

    XSendEvent(x11Helper.display, request->requestor, False, 0, &reply);
}

bool WindowIsHandle(WindowX11 *window, long unsigned int id){
    Window win = (Window) id;
    return window->handle == win;
}

void ProcessEventFocusInX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    List_ForAllItems(&window->onFocusChangeCall, [=](OnFocusChangeCallback *sc){
        sc->fn(true, event->xfocus.window, sc->priv);
    });
}

void ProcessEventFocusOutX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    SkipEventNoWindow(window);
    List_ForAllItems(&window->onFocusChangeCall, [=](OnFocusChangeCallback *sc){
        sc->fn(false, event->xfocus.window, sc->priv);
    });
}

void ProcessEventExposeX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //TODO: ??????????
}

void ProcessEventPropertyX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Property Notify\n");
}

void ProcessEventDestroyX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Destroy Notify\n");
}

// TODO: We need to add support for Xdnd so we can get drag and drop working for linux
void ProcessEventX11(XEvent *event){
    WindowX11 *window = NULL;
    int filtered = XFilterEvent(event, None);
    XFindContext(x11Helper.display, event->xany.window,
                 x11Helper.context, (XPointer *)&window);

    switch(event->type){
        case ReparentNotify:{
            ProcessEventReparentX11(event, window, &x11Helper);
        } break;

        case KeyPress:{
            ProcessEventKeyPressX11(event, window, &x11Helper);
        } break;

        case KeyRelease:{
            ProcessEventKeyReleaseX11(event, window, &x11Helper);
        } break;

        case ButtonPress:{
            ProcessEventButtonPressX11(event, window, &x11Helper);
        } break;

        case ButtonRelease:{
            ProcessEventButtonReleaseX11(event, window, &x11Helper);
        } break;

        case EnterNotify:{
            ProcessEventEnterX11(event, window, &x11Helper);
        } break;

        case LeaveNotify:{
            ProcessEventLeaveX11(event, window, &x11Helper);
        } break;

        case MotionNotify:{
            ProcessEventMotionX11(event, window, &x11Helper);
        } break;

        case ConfigureNotify:{
            ProcessEventConfigureX11(event, window, &x11Helper);
        } break;

        case ClientMessage:{
            if(filtered) return;
            ProcessEventClientMessageX11(event, window, &x11Helper);
        } break;

        case SelectionNotify:{
            ProcessEventSelectionX11(event, window, &x11Helper);
        } break;

        case SelectionClear:{
            ProcessEventSelectionClearX11(event, window, &x11Helper);
        } break;

        case SelectionRequest:{
            ProcessEventSelectionRequestX11(event, window, &x11Helper);
        } break;

        case FocusIn:{
            ProcessEventFocusInX11(event, window, &x11Helper);
        } break;

        case FocusOut:{
            ProcessEventFocusOutX11(event, window, &x11Helper);
        } break;

        case Expose:{
            ProcessEventExposeX11(event, window, &x11Helper);
        } break;

        case PropertyNotify:{
            ProcessEventPropertyX11(event, window, &x11Helper);
        } break;

        case DestroyNotify:{
            ProcessEventDestroyX11(event, window, &x11Helper);
        } break;

        default:{
            //printf("Pending X11 event %d\n", (int)event->type);
        }
    }
}
