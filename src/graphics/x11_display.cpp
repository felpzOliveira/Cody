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

LibHelperX11 x11Helper;
Timer timer;
Framebuffer x11Framebuffer;
ContextGL x11GLContext;

void ProcessEventX11(XEvent *event);

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
    AssertErr(InitGLX(x11) == 1, "Failed to initialize GLX");
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

void RegisterOnMouseMotionCallback(WindowX11 *window, onMouseMotionCallback *callback){
    window->onMouseMotionCall = callback;
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
    
    if(request->selection == x11Helper.PRIMARY)
        selectionString = x11Helper.primarySelectionString;
    else
        selectionString = x11Helper.clipboardString;
    
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
                                strlen(selectionString));
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
                            strlen(selectionString));
            
            return request->property;
        }
    }
    
    // The requested target is not supported
    
    return None;
}

const char* GetSelectionString(Atom selection){
    char** selectionString = NULL;
    const Atom targets[] = { x11Helper.UTF8_STRING, XA_STRING };
    const size_t targetCount = sizeof(targets) / sizeof(targets[0]);
    
    if(selection == x11Helper.PRIMARY)
        selectionString = &x11Helper.primarySelectionString;
    else
        selectionString = &x11Helper.clipboardString;
    
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
    
    return *selectionString;
    
}

const char *ClipboardGetStringX11(unsigned int *size){
    const char *str = GetSelectionString(x11Helper.CLIPBOARD);
    *size = strlen(str); // TODO: Make this thing go away
    return str;
}

void ClipboardSetStringX11(char *str){
    free(x11Helper.clipboardString);
    x11Helper.clipboardString = str;
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

void WaitForEventsX11(){
    while(!XPending(x11Helper.display))
        waitForEvent(NULL);
    
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
    if(window->onMouseMotionCall){
        window->onMouseMotionCall(x, y);
    }
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
    XFindContext(x11Helper.display, event->xany.window,
                 x11Helper.context, (XPointer *)&window);
    //if(rv != 0) return;
    
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
            printf("Pending X11 event %d\n", (int)event->type);
        }
    }
}
