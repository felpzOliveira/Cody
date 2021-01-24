#include <x11_display.h>
#include <X11/Xresource.h>
#include <types.h>
#include <utilities.h>
#include <time.h>
#include <string.h>
#include <glx_helper.h>
#include <unistd.h>
#include <sys/time.h>

LibHelperX11 x11Helper;
Timer timer;
Framebuffer x11Framebuffer;
ContextGL x11GLContext;

void InitializeTimer(){
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == 0){
        timer.monotonic = 1;
        timer.frequency = 1000000000;
    }else{
        timer.monotonic = 0;
        timer.frequency = 1000000;
    }
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

uint64_t GetTimerFrequency(){
    return timer.frequency;
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

vec2f GetSystemContentScale(Display *display){
    Float xdpi = 96.0f, ydpi = 96.0f;
    char *rms = XResourceManagerString(display);
    if(rms){
        XrmDatabase xrmDb = XrmGetStringDatabase(rms);
        if(xrmDb){
            XrmValue value;
            char *type = nullptr;
            if(XrmGetResource(xrmDb, "Xft.dpi", "Xft.Dpi", &type, &value)){
                if(type && StringEqual(type, (char *)"String", 6)){
                    xdpi = ydpi = atof(value.addr);
                }
            }
            
            XrmDestroyDatabase(xrmDb);
        }
    }
    
    return vec2f(xdpi / 96.0f, ydpi / 96.0f);
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
    
    printf("XKB: %d.%d\n", x11->xkb.major, x11->xkb.minor);
    
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
    
    vec2f scaleDpi = GetSystemContentScale(x11Helper.display);
    scaleDpi.PrintSelf();
    
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
    
    x11Helper.WM_DELETE_WINDOW = XInternAtom(x11Helper.display, "WM_DELETE_WINDOW", 0);
    x11Helper.NET_WM_PING = XInternAtom(x11Helper.display, "_NET_WM_PING", 0);
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

void CreateWindowX11(int width, int height, const char *title,
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
    
    window->width  = width;
    window->height = height;
    context->window = (WindowX11 *)window;
    
    (void)CreateContextGLX(context, fb, x11);
    
    MakeContextCurrentGLX((void *)window);
    
    XMapWindow(x11->display, window->handle);
    waitForVisibilityNotify(window, x11);
    
    XFlush(x11->display);
}

void test_x11_entry(WindowX11 *window){
    memset(window, 0x00, sizeof(WindowX11));
    
    InitializeX11();
    x11Framebuffer.samples = 16;
    x11GLContext.major = 3;
    x11GLContext.minor = 3;
    x11GLContext.forward = 1;
    x11GLContext.profile = OPENGL_CORE_PROFILE;
    
    CreateWindowX11(1600, 900, "Test-window", &x11Framebuffer, 
                    &x11GLContext, &x11Helper, window);
}

void swap_buffers(WindowX11 *window){
    SwapBufferGLX((void *)window);
}

void pool_events(){
    XPending(x11Helper.display);
    
    while(XQLength(x11Helper.display)){
        XEvent event;
        XNextEvent(x11Helper.display, &event);
        int filtered = XFilterEvent(&event, None);
        //process_event(&event);
    }
    
    //TODO: Figure out this cursor thingy in glfw
    
    XFlush(x11Helper.display);
}