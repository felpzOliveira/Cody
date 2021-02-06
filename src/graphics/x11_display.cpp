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

LibHelperX11 x11Helper;
Timer timer;
Framebuffer x11Framebuffer;
ContextGL x11GLContext;

void ProcessEventX11(XEvent *event);

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
    
    AssertA(x11Helper.display != NULL, "Failed to connect to X");
    
    x11Helper.screen  = DefaultScreen(x11Helper.display);
    x11Helper.root    = RootWindow(x11Helper.display, x11Helper.screen);
    x11Helper.context = XUniqueContext();
    
    XkbSetup(&x11Helper);
    x11Helper.helperWindow = CreateHelperWindow(&x11Helper);
    x11Helper.hiddenCursor = None; //TODO
    
    AssertA(setlocale(LC_ALL, "") != NULL, "Failed to set locale");
    AssertA(XSupportsLocale(), "X does not supporte locale");
    AssertA(XSetLocaleModifiers("@im=none") != NULL, "Failed to set locale modifiers");
    
    x11Helper.im = XOpenIM(x11Helper.display, 0, NULL, NULL);
    AssertA(x11Helper.im != NULL, "Failed to open input method");
    
    AssertA(XIMHasInputStyles(&x11Helper) != 0, "No input styles found");
    
    InitializeTimer();
    InitializeFramebuffer(&x11Framebuffer);
    
    //TODO: Expose?
    x11GLContext.forward = 1;
    x11GLContext.profile = OPENGL_CORE_PROFILE;
    
    x11Helper.WM_DELETE_WINDOW = XInternAtom(x11Helper.display, "WM_DELETE_WINDOW", 0);
    x11Helper.NET_WM_PING = XInternAtom(x11Helper.display, "_NET_WM_PING", 0);
}

int WindowShouldCloseX11(WindowX11 *window){
    return window->shouldClose;
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

void _CreateWindowX11(int width, int height, const char *title,
                      Framebuffer *fb, ContextGL *context, LibHelperX11 *x11,
                      WindowX11 *window)
{
    Visual *visual;
    int depth = 0;
    AssertA(InitGLX(x11) == 1, "Failed to initialize GLX");
    ChooseVisualGLX(fb, x11, &visual, &depth);
    
    window->colormap = XCreateColormap(x11->display, x11->root, visual, AllocNone);
    XSetWindowAttributes wa = { 0 };
    
    wa.colormap = window->colormap;
    wa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
        PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
        ExposureMask | FocusChangeMask | VisibilityChangeMask |
        EnterWindowMask | LeaveWindowMask | PropertyChangeMask;
    
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
    
    window->shouldClose = 0;
    
    XFlush(x11->display);
}

void SetSamplesX11(int samples){
    x11Framebuffer.samples = Max(0, samples);
}

void SetOpenGLVersionX11(int major, int minor){
    x11GLContext.major = major;
    x11GLContext.minor = minor;
}

void RegisterOnScrollCallback(WindowX11 *window, onScrollCallback *callback){
    window->onScrollCall = callback;
}

void RegisterOnMouseClickCallback(WindowX11 *window, onMouseClickCallback *callback){
    window->onMouseClickCall = callback;
}

void RegisterOnSizeChangeCallback(WindowX11 *window, onSizeChangeCallback *callback){
    window->onSizeChangeCall = callback;
}

void RegisterOnFocusChangeCallback(WindowX11 *window, onFocusChangeCallback *callback){
    window->onFocusChangeCall = callback;
}

WindowX11 *CreateWindowX11(int width, int height, const char *title){
    WindowX11 *window = (WindowX11 *)calloc(1, sizeof(WindowX11));
    _CreateWindowX11(width, height, title, &x11Framebuffer,
                     &x11GLContext, &x11Helper, window);
    
    //TODO: Set callbacks to empty
    window->onScrollCall = NULL;
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
        
        free(window);
    }
}

void SwapIntervalX11(WindowX11 *window, int interval){
    SwapIntervalGLX((void *)window, interval);
}

void SwapBuffersX11(WindowX11 *window){
    SwapBufferGLX((void *)window);
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
    window->parent = event->xreparent.parent;
}

void ProcessEventKeyPressX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    int count = 0;
    int code = event->xkey.keycode;
    KeySym keysym = 0;
    char buf[20];
    Status status = 0;
    
    // Somehow we getting duplicated events lets filter them out
    if(!(window->lastKeyTime < event->xkey.time)) return;
    
    int mappedKey = TranslateKeyX11(code);
    count = Xutf8LookupString(window->ic, (XKeyPressedEvent*)event, buf, 
                              20, &keysym, &status);
    
    if(status == XBufferOverflow){
        printf("BufferOverflow\n");
        AssertA(0, "Weird UTF8 character input");
    }
    
    KeyboardEvent((Key)mappedKey, KEYBOARD_EVENT_PRESS, buf, count, code);
    
    window->lastKeyTime = event->xkey.time;
}

void ProcessEventKeyReleaseX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    int code = event->xkey.keycode;
    int mappedKey = TranslateKeyX11(code);
    KeyboardEvent((Key)mappedKey, KEYBOARD_EVENT_RELEASE, NULL, 0, code);
}

void ProcessEventButtonPressX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
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
            if(window->onMouseClickCall){
                window->onMouseClickCall(window->lastCursorPosX,
                                         window->lastCursorPosY);
            }
        } break;
        default:{ }
    }
    
    if(is_scroll && window->onScrollCall){
        window->onScrollCall(is_scroll_up);
    }
}

void ProcessEventButtonReleaseX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Button Release\n");
}

void ProcessEventEnterX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Enter Notify\n");
}

void ProcessEventLeaveX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Leave Notify\n");
}

void ProcessEventMotionX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    int x = event->xmotion.x;
    int y = event->xmotion.y;
    window->lastCursorPosX = x;
    window->lastCursorPosY = y;
}

void ProcessEventConfigureX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    if(event->xconfigure.width != window->width ||
       event->xconfigure.height != window->height)
    {
        window->width = event->xconfigure.width;
        window->height = event->xconfigure.height;
        
        if(window->onSizeChangeCall){
            window->onSizeChangeCall(window->width, window->height);
        }
    }
}

void ProcessEventClientMessageX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //TODO: Protocols
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
    //printf("Selection Notify\n");
}

void ProcessEventFocusInX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Focus IN\n");
    if(window->onFocusChangeCall){
        window->onFocusChangeCall();
    }
}

void ProcessEventFocusOutX11(XEvent *event, WindowX11 *window, LibHelperX11 *x11){
    //printf("Focus OUT\n");
    if(window->onFocusChangeCall){
        window->onFocusChangeCall();
    }
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

void ProcessEventX11(XEvent *event){
    WindowX11 *window = NULL;
    int filtered = XFilterEvent(event, None);
    int rv = XFindContext(x11Helper.display, event->xany.window,
                          x11Helper.context, (XPointer *)&window);
    if(rv != 0) return;
    
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
            printf("Pending\n");
        }
    }
}