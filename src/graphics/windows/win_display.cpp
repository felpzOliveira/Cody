#include <glad/glad.h>
#include <win_display.h>
#include <Windows.h>
#include <stdio.h>
#include <winuser.h>
#include <stdint.h>
#include <keyboard.h>
#include <utilities.h>
#include <iostream>
#include <wgl_helper.h>
#include <timer.h>

#define DISPLAY_CLASS_NAME L"SomeClassName"
#define SkipEventNoWindow(window) do{ if(!window) return; }while(0)

#if 0
    #define STARTED() printf(" > Calling %s\n", __FUNCTION__)
    #define FINISHED() printf(" > Finished %s\n", __FUNCTION__)
#else
    #define STARTED()
    #define FINISHED()
#endif

#define ERROR_MSG(...) do{printf(" * ERROR: "); printf(__VA_ARGS__); printf("\n"); }while(0)

static const GUID GUID_DEVINTERFACE_HID =
{ 0x4d1e55b2,0xf16f,0x11cf,{0x88,0xcb,0x00,0x11,0x11,0x00,0x00,0x30} };

LibHelperWin32 win32Helper;
Framebuffer win32Framebuffer;
ContextGL win32GLContext;

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
    type __rc_cc;\
    __rc_cc.fn = cb;\
    __rc_cc.priv = priv;\
    __rc_cc.handle = __rc_handle;\
    List_Push(&(wnd)->field, &__rc_cc);\
    return __rc_handle;\
}while(0)

#define UnregisterCallback(wnd, hnd, field, type) do{\
    auto finder = [&](type *item) -> int{\
        return (item->handle == hnd ? 1 : 0);\
    };\
    List_Erase(&(wnd)->field, finder);\
}while(0)

#include <sstream>
std::string ConvertLPWSTRToStdString(LPWSTR lpwszStr) {
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, lpwszStr, -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded == 0) {
        // Error handling
        return "";
    }

    std::string str(sizeNeeded, '\0');
    WideCharToMultiByte(CP_UTF8, 0, lpwszStr, -1, &str[0], sizeNeeded, nullptr, nullptr);

    return str;
}

void log_last_error(){
    DWORD errorCode = GetLastError();
    LPWSTR errorMessage = nullptr;

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&errorMessage),
        0, NULL
    );

    if(errorMessage != nullptr){
        // Display the error message
        std::stringstream ss;
        ss << "Error code " << errorCode << ": " << ConvertLPWSTRToStdString(errorMessage);
        LocalFree(errorMessage); // Free the error message buffer
        ERROR_MSG("%s", ss.str().c_str());
    }else{
        // Failed to retrieve error message
        ERROR_MSG("Error occurred, but failed to retrieve error message");
    }
}

int __CodepointToString(int cp, char* c) {
    int n = -1;
    memset(c, 0x00, sizeof(char) * 5);
    if (cp <= 0x7F) {
        c[0] = cp;
        n = 1;
    }
    else if (cp <= 0x7FF) {
        c[0] = (cp >> 6) + 192;
        c[1] = (cp & 63) + 128;
        n = 2;
    }
    else if (0xd800 <= cp && cp <= 0xdfff) {
        n = 0;
        //invalid block of utf8
    }
    else if (cp <= 0xFFFF) {
        c[0] = (cp >> 12) + 224;
        c[1] = ((cp >> 6) & 63) + 128;
        c[2] = (cp & 63) + 128;
        n = 3;
    }
    else if (cp <= 0x10FFFF) {
        c[0] = (cp >> 18) + 240;
        c[1] = ((cp >> 12) & 63) + 128;
        c[2] = ((cp >> 6) & 63) + 128;
        c[3] = (cp & 63) + 128;
        n = 4;
    }
    return n;
}

char* CreateUTF8FromWideStringWin32(const WCHAR * source){
    char* target;
    int size;

    size = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!size){
        ERROR_MSG("Win32: Failed to convert string to UTF-8");
        return NULL;
    }

    target = (char *)calloc(size, 1);

    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, size, NULL, NULL)){
        ERROR_MSG("Win32: Failed to convert string to UTF-8");
        free(target);
        return NULL;
    }

    return target;
}

void InitializeFramebuffer(Framebuffer *framebuffer) {
    memset(framebuffer, 0x0, sizeof(Framebuffer));
    framebuffer->redBits = 8;
    framebuffer->greenBits = 8;
    framebuffer->blueBits = 8;
    framebuffer->alphaBits = 8;
    framebuffer->depthBits = 24;
    framebuffer->stencilBits = 8;
    framebuffer->doublebuffer = 1;
}

static bool loadLibraries(){
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (const WCHAR*)&win32Helper,
        (HMODULE*)&win32Helper.instance))
    {
        printf("Win32: Failed to retrieve own module handle\n");
        return false;
    }

    win32Helper.user32.instance = LoadLibraryA("user32.dll");
    if (!win32Helper.user32.instance){
        printf("Win32: Failed to load user32.dll");
        return false;
    }

    win32Helper.user32.SetProcessDPIAware_ = (PFN_SetProcessDPIAware)
        GetProcAddress(win32Helper.user32.instance, "SetProcessDPIAware");
    win32Helper.user32.ChangeWindowMessageFilterEx_ = (PFN_ChangeWindowMessageFilterEx)
        GetProcAddress(win32Helper.user32.instance, "ChangeWindowMessageFilterEx");
    win32Helper.user32.EnableNonClientDpiScaling_ = (PFN_EnableNonClientDpiScaling)
        GetProcAddress(win32Helper.user32.instance, "EnableNonClientDpiScaling");
    win32Helper.user32.SetProcessDpiAwarenessContext_ = (PFN_SetProcessDpiAwarenessContext)
        GetProcAddress(win32Helper.user32.instance, "SetProcessDpiAwarenessContext");
    win32Helper.user32.GetDpiForWindow_ = (PFN_GetDpiForWindow)
        GetProcAddress(win32Helper.user32.instance, "GetDpiForWindow");
    win32Helper.user32.AdjustWindowRectExForDpi_ = (PFN_AdjustWindowRectExForDpi)
        GetProcAddress(win32Helper.user32.instance, "AdjustWindowRectExForDpi");
    win32Helper.user32.GetSystemMetricsForDpi_ = (PFN_GetSystemMetricsForDpi)
        GetProcAddress(win32Helper.user32.instance, "GetSystemMetricsForDpi");

    win32Helper.dinput8.instance = LoadLibraryA("dinput8.dll");
    if (win32Helper.dinput8.instance){
        win32Helper.dinput8.Create = (PFN_DirectInput8Create)
            GetProcAddress(win32Helper.dinput8.instance, "DirectInput8Create");
    }

    {
        int i;
        const char* names[] =
        {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll",
            "xinput1_2.dll",
            "xinput1_1.dll",
            NULL
        };

        for (i = 0; names[i]; i++){
            win32Helper.xinput.instance = LoadLibraryA(names[i]);
            if (win32Helper.xinput.instance){
                win32Helper.xinput.GetCapabilities = (PFN_XInputGetCapabilities)
                    GetProcAddress(win32Helper.xinput.instance, "XInputGetCapabilities");
                win32Helper.xinput.GetState = (PFN_XInputGetState)
                    GetProcAddress(win32Helper.xinput.instance, "XInputGetState");

                break;
            }
        }
    }

    win32Helper.dwmapi.instance = LoadLibraryA("dwmapi.dll");
    if (win32Helper.dwmapi.instance)
    {
        win32Helper.dwmapi.IsCompositionEnabled = (PFN_DwmIsCompositionEnabled)
            GetProcAddress(win32Helper.dwmapi.instance, "DwmIsCompositionEnabled");
        win32Helper.dwmapi.Flush = (PFN_DwmFlush)
            GetProcAddress(win32Helper.dwmapi.instance, "DwmFlush");
        win32Helper.dwmapi.EnableBlurBehindWindow = (PFN_DwmEnableBlurBehindWindow)
            GetProcAddress(win32Helper.dwmapi.instance, "DwmEnableBlurBehindWindow");
        win32Helper.dwmapi.GetColorizationColor = (PFN_DwmGetColorizationColor)
            GetProcAddress(win32Helper.dwmapi.instance, "DwmGetColorizationColor");
    }

    win32Helper.shcore.instance = LoadLibraryA("shcore.dll");
    if (win32Helper.shcore.instance)
    {
        win32Helper.shcore.SetProcessDpiAwareness_ = (PFN_SetProcessDpiAwareness)
            GetProcAddress(win32Helper.shcore.instance, "SetProcessDpiAwareness");
        win32Helper.shcore.GetDpiForMonitor_ = (PFN_GetDpiForMonitor)
            GetProcAddress(win32Helper.shcore.instance, "GetDpiForMonitor");
    }

    win32Helper.ntdll.instance = LoadLibraryA("ntdll.dll");
    if (win32Helper.ntdll.instance){
        win32Helper.ntdll.RtlVerifyVersionInfo_ = (PFN_RtlVerifyVersionInfo)
            GetProcAddress(win32Helper.ntdll.instance, "RtlVerifyVersionInfo");
    }

    return true;
}

BOOL IsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp) {
    OSVERSIONINFOEXW osvi = { sizeof(osvi), major, minor, 0, 0, {0}, sp };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

BOOL IsWindows10BuildOrGreaterWin32(WORD build) {
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 10, 0, build };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

static bool createHelperWindow(){
    MSG msg;

    win32Helper.helperWindowHandle =
        CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
            DISPLAY_CLASS_NAME, // TODO
            L"Message window",
            WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 1, 1,
            NULL, NULL,
            win32Helper.instance,
            NULL);

    if (!win32Helper.helperWindowHandle){
       ERROR_MSG("Win32: Failed to create helper window");
        return false;
    }

    // HACK: The command to the first ShowWindow call is ignored if the parent
    //       process passed along a STARTUPINFO, so clear that with a no-op call
    ShowWindow(win32Helper.helperWindowHandle, SW_HIDE);

    // Register for HID device notifications
    {
        DEV_BROADCAST_DEVICEINTERFACE_W dbi;
        ZeroMemory(&dbi, sizeof(dbi));
        dbi.dbcc_size = sizeof(dbi);
        dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbi.dbcc_classguid = GUID_DEVINTERFACE_HID;

        win32Helper.deviceNotificationHandle =
            RegisterDeviceNotificationW(win32Helper.helperWindowHandle,
                (DEV_BROADCAST_HDR*)&dbi,
                DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    while (PeekMessageW(&msg, win32Helper.helperWindowHandle, 0, 0, PM_REMOVE)){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

void SetClipboardStringWin32(const char* string){
    int characterCount;
    HANDLE object;
    WCHAR* buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    if (!characterCount)
        return;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object){
        ERROR_MSG("Win32: Failed to allocate global handle for clipboard");
        return;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer){
        ERROR_MSG("Win32: Failed to lock global handle");
        GlobalFree(object);
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, string, -1, buffer, characterCount);
    GlobalUnlock(object);

    if (!OpenClipboard(win32Helper.helperWindowHandle)){
        ERROR_MSG("Win32: Failed to open clipboard");
        GlobalFree(object);
        return;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();
}

const char* GetClipboardStringWin32(int *size){
    HANDLE object;
    WCHAR* buffer;

    if (!OpenClipboard(win32Helper.helperWindowHandle)){
        ERROR_MSG("Win32: Failed to open clipboard");
        return NULL;
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object){
        ERROR_MSG("Win32: Failed to convert clipboard to string");
        CloseClipboard();
        return NULL;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer){
        ERROR_MSG("Win32: Failed to lock global handle");
        CloseClipboard();
        return NULL;
    }

    free(win32Helper.clipboardString);
    win32Helper.clipboardString = CreateUTF8FromWideStringWin32(buffer);
    if(win32Helper.clipboardString)
        *size = strlen(win32Helper.clipboardString);

    GlobalUnlock(object);
    CloseClipboard();

    return win32Helper.clipboardString;
}

void ProcessEventFocusWin32(WindowWin32* window, LibHelperWin32* win32){
    SkipEventNoWindow(window);
    List_ForAllItems(&window->onFocusChangeCall, [=](OnFocusChangeCallback* sc){
        sc->fn(true, (long unsigned int)window->handle, sc->priv);
    });
}

void ProcessEventFocusOutWin32(WindowWin32* window, LibHelperWin32* x11){
    SkipEventNoWindow(window);
    List_ForAllItems(&window->onFocusChangeCall, [=](OnFocusChangeCallback* sc) {
        sc->fn(false, (long unsigned int)window->handle, sc->priv);
    });
}

void ProcessEventButtonPressWin32(WindowWin32* window, LibHelperWin32* win32, UINT uMsg){
    SkipEventNoWindow(window);
    switch (uMsg) {
        case WM_LBUTTONDOWN: {
            window->isMousePressed = 1;
            List_ForAllItems(&window->onMousePressedCall, [=](OnMousePressedCallback* sc) {
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });
        } break;
        // TODO: Other types?
        default: { }
    }
}

void ProcessEventButtonReleaseWin32(WindowWin32* window, LibHelperWin32* win32, UINT uMsg){
    SkipEventNoWindow(window);
    switch (uMsg) {
        case WM_LBUTTONUP: {
            window->isMousePressed = 0;
            List_ForAllItems(&window->onMouseReleasedCall, [=](OnMouseReleasedCallback* sc) {
                sc->fn(sc->priv);
            });

            List_ForAllItems(&window->onMouseLClickCall, [=](OnMouseLClickCallback* sc) {
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });

            // I really hate that I have to do this, because all I wanted
            // was to be happy. But aparently linux does not want me
            // to be happy.
            DWORD messageTime = GetMessageTime();
            double dif = messageTime - window->lastClickTime;
            double xdif = window->xpos - window->lastCursorPosX;
            double ydif = window->ypos - window->lastCursorPosY;
            double dist = std::sqrt(xdif * xdif + ydif * ydif);

            if (dif < MOUSE_DCLICK_INTERVAL && dist < MOUSE_DCLICK_DISTANCE) {
                List_ForAllItems(&window->onMouseDClickCall, [=](OnMouseDClickCallback* sc) {
                    sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
                });
            }
            window->lastClickTime = messageTime;
            window->xpos = window->lastCursorPosX;
            window->ypos = window->lastCursorPosY;
        } break;
        case WM_RBUTTONUP: {
            List_ForAllItems(&window->onMouseRClickCall, [=](OnMouseRClickCallback* sc) {
                sc->fn(window->lastCursorPosX, window->lastCursorPosY, sc->priv);
            });
        } break;
        default: { }
    }
}

void ProcessEventMotionWin32(WindowWin32* window, LibHelperWin32* win32, int x, int y){
    SkipEventNoWindow(window);
    window->lastCursorPosX = x;
    window->lastCursorPosY = y;
    List_ForAllItems(&window->onMouseMotionCall, [=](OnMouseMotionCallback* sc) {
        sc->fn(x, y, sc->priv);
    });
}

HICON LoadPNGToIcon(const char *path){
    return reinterpret_cast<HICON>(LoadImageA(nullptr, path, IMAGE_ICON,
                                              0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
}

void SetWindowIconWin32(WindowWin32 *window, const char *iconPngPath){
    HICON hCustomIcon = LoadPNGToIcon(iconPngPath);
    if(hCustomIcon){
        SendMessage(window->handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hCustomIcon));
        SendMessage(window->handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hCustomIcon));
    }else{
        ERROR_MSG("Could not get icon image");
        log_last_error();
    }
}

static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam)
{
    WindowWin32* window = (WindowWin32 *)GetPropW(hWnd, DISPLAY_CLASS_NAME);
    if (!window){
        // This is the message handling for the hidden helper window
        // and for a regular window during its initial creation

        switch (uMsg){
            // NOTE: Received prior to window being created, nothing to do
            case WM_NCCREATE: { }break;
            // NOTE: Received when the display resolution is changed... is this the monitor?
            case WM_DISPLAYCHANGE: { } break;
            // NOTE: Received when hardware device configuration changed?
            case WM_DEVICECHANGE: { } break;
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg){
        // NOTE: Received when cursor is in a inactive window????
        case WM_MOUSEACTIVATE: { } break;
        // NOTE: Received when the window looses the mouse capture???
        case WM_CAPTURECHANGED: { } break;
        // NOTE: Received when we get focus
        case WM_SETFOCUS: {
            ProcessEventFocusWin32(window, &win32Helper);
            return 0;
        }
        // NOTE: Received when we lose focus
        case WM_KILLFOCUS: {
            ProcessEventFocusOutWin32(window, &win32Helper);
            return 0;
        }
        // NOTE: Received when user uses command form menu???
        case WM_SYSCOMMAND: {
            switch (wParam & 0xfff0) {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER: {
                    return 0;
                }

                // User trying to access application menu using ALT?
                case SC_KEYMENU:
                    return 0;
            } break;
        }

        case WM_CLOSE: {
            SetWindowShouldCloseWin32(window);
            return 0;
        }
        // NOTE: Received when language changes??
        case WM_INPUTLANGCHANGE: { } break;

        case WM_CHAR:
        case WM_SYSCHAR: {
            if (wParam >= 0xd800 && wParam <= 0xdbff)
                window->highSurrogate = (WCHAR)wParam;
            else {
                uint32_t codepoint = 0;
                char buf[5];

                if (wParam >= 0xdc00 && wParam <= 0xdfff) {
                    if (window->highSurrogate) {
                        codepoint += (window->highSurrogate - 0xd800) << 10;
                        codepoint += (WCHAR)wParam - 0xdc00;
                        codepoint += 0x10000;
                    }
                }
                else
                    codepoint = (WCHAR)wParam;

                window->highSurrogate = 0;

                int count = __CodepointToString(codepoint, buf);

                int scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
                int mappedKey = TranslateKey(scancode);

                DWORD dif = GetMessageTime() - window->lastKeyTime;
                if(dif < 2 && scancode == window->lastKeyScancode){
                    return 0;
                }

                KeyboardEvent((Key)mappedKey, KEYBOARD_EVENT_PRESS, buf, count,
                             scancode, (void*)window, 1);
            }
            return 0;
        }

        case WM_UNICHAR:{
            if (wParam == UNICODE_NOCHAR){
                // WM_UNICHAR is not sent by Windows, but is sent by some
                // third-party input method engine
                // Returning TRUE here announces support for this message
                return TRUE;
            }
            // TODO: Should we actually translate this?
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:{
            int key, scancode;
            const int action = (HIWORD(lParam) & KF_UP) ? KEYBOARD_EVENT_RELEASE : KEYBOARD_EVENT_PRESS;

            scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
            if (!scancode){
                // NOTE: Some synthetic key messages have a scancode of zero
                // HACK: Map the virtual key back to a usable scancode
                scancode = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_VSC);
            }

            // HACK: Alt+PrtSc has a different scancode than just PrtSc
            if (scancode == 0x54)
                scancode = 0x137;

            // HACK: Ctrl+Pause has a different scancode than just Pause
            if (scancode == 0x146)
                scancode = 0x45;

            // HACK: CJK IME sets the extended bit for right Shift
            if (scancode == 0x136)
                scancode = 0x36;

            key = TranslateKey(scancode);

            // The Ctrl keys require special handling
            if (wParam == VK_CONTROL){
                if (HIWORD(lParam) & KF_EXTENDED){
                    // Right side keys have the extended key bit set
                    key = Key_RightControl;
                }else{
                    // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                    // HACK: We only want one event for Alt Gr, so if we detect
                    //       this sequence we discard this Left Ctrl message now
                    //       and later report Right Alt normally
                    MSG next;
                    const DWORD time = GetMessageTime();

                    if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE)){
                        if (next.message == WM_KEYDOWN ||
                            next.message == WM_SYSKEYDOWN ||
                            next.message == WM_KEYUP ||
                            next.message == WM_SYSKEYUP)
                        {
                            if (next.wParam == VK_MENU &&
                                (HIWORD(next.lParam) & KF_EXTENDED) &&
                                next.time == time)
                            {
                                // Next message is Right Alt down so discard this
                                break;
                            }
                        }
                    }

                    // This is a regular Left Ctrl message
                    key = Key_LeftControl;
                }
            }

            {
                // NOTE: The content of the utf8 buffer doesn' matter since we are not allowing
                //       key forwarding here
                KeyboardRegisterKeyState((Key)key, KEYBOARD_EVENT_RELEASE);
            }

            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:{
            int key, scancode;
            const int action = (HIWORD(lParam) & KF_UP) ? KEYBOARD_EVENT_RELEASE : KEYBOARD_EVENT_PRESS;

            scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
            if (!scancode){
                // NOTE: Some synthetic key messages have a scancode of zero
                // HACK: Map the virtual key back to a usable scancode
                scancode = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_VSC);
            }

            // HACK: Alt+PrtSc has a different scancode than just PrtSc
            if (scancode == 0x54)
                scancode = 0x137;

            // HACK: Ctrl+Pause has a different scancode than just Pause
            if (scancode == 0x146)
                scancode = 0x45;

            // HACK: CJK IME sets the extended bit for right Shift
            if (scancode == 0x136)
                scancode = 0x36;

            key = TranslateKey(scancode);

            // The Ctrl keys require special handling
            if (wParam == VK_CONTROL){
                if (HIWORD(lParam) & KF_EXTENDED){
                    // Right side keys have the extended key bit set
                    key = Key_RightControl;
                }else{
                    // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                    // HACK: We only want one event for Alt Gr, so if we detect
                    //       this sequence we discard this Left Ctrl message now
                    //       and later report Right Alt normally
                    MSG next;
                    const DWORD time = GetMessageTime();

                    if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE)){
                        if (next.message == WM_KEYDOWN ||
                            next.message == WM_SYSKEYDOWN ||
                            next.message == WM_KEYUP ||
                            next.message == WM_SYSKEYUP)
                        {
                            if (next.wParam == VK_MENU &&
                                (HIWORD(next.lParam) & KF_EXTENDED) &&
                                next.time == time)
                            {
                                // Next message is Right Alt down so discard this
                                break;
                            }
                        }
                    }

                    // This is a regular Left Ctrl message
                    key = Key_LeftControl;
                }
            }else if (wParam == VK_PROCESSKEY){
                // IME notifies that keys have been filtered by setting the
                // virtual key-code to VK_PROCESSKEY
                break;
            }

            {
                // NOTE: The content of the utf8 buffer doesn' matter since we are not allowing
                //       key forwarding here

                KeyboardRegisterKeyState((Key)key, KEYBOARD_EVENT_PRESS);
                window->lastKeyScancode = 0;
                if(KeyboardAttemptToConsumeKey((Key)key, window)){
                    window->lastKeyTime = GetMessageTime();
                    window->lastKeyScancode = scancode;
                }
            }

            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:{
            ProcessEventButtonPressWin32(window, &win32Helper, uMsg);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:{
            ProcessEventButtonReleaseWin32(window, &win32Helper, uMsg);
            return 0;
        }

        case WM_MOUSEMOVE:{
            const int x = LOWORD(lParam);
            const int y = HIWORD(lParam);
            ProcessEventMotionWin32(window, &win32Helper, x, y);
            return 0;
        }

        case WM_INPUT:{
            ERROR_MSG("Got WM_INPUT");
#if 0
        UINT size = 0;
        HRAWINPUT ri = (HRAWINPUT)lParam;
        RAWINPUT* data = NULL;
        int dx, dy;

        if (_glfw.win32.disabledCursorWindow != window)
            break;
        if (!window->rawMouseMotion)
            break;

        GetRawInputData(ri, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
        if (size > (UINT)win32Helper.rawInputSize)
        {
            free(win32Helper.rawInput);
            win32Helper.rawInput = (RAWINPUT *)calloc(size, 1);
            win32Helper.rawInputSize = size;
        }

        size = win32Helper.rawInputSize;
        if (GetRawInputData(ri, RID_INPUT,
            win32Helper.rawInput, &size,
            sizeof(RAWINPUTHEADER)) == (UINT)-1)
        {
            ERROR_MSG("Win32: Failed to retrieve raw input data");
            break;
        }

        data = win32Helper.rawInput;
        if (data->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
        {
            dx = data->data.mouse.lLastX - window->lastCursorPosX;
            dy = data->data.mouse.lLastY - window->lastCursorPosY;
        }
        else
        {
            dx = data->data.mouse.lLastX;
            dy = data->data.mouse.lLastY;
        }

        //ERROR_MSG("MOUSE MOTION?");
        //_glfwInputCursorPos(window window->virtualCursorPosX + dx, window->virtualCursorPosY + dy);

        window->lastCursorPosX += dx;
        window->lastCursorPosY += dy;
#endif
            break;
        }

        case WM_MOUSELEAVE: {
            return 0;
        } break;

        case WM_MOUSEWHEEL:{
            short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            int is_scroll_up = wheelDelta > 0 ? 1 : 0;
            List_ForAllItems(&window->onScrollCall, [=](OnScrollCallback* sc) {
                sc->fn(is_scroll_up, sc->priv);
            });
            return 0;
        }

        case WM_MOUSEHWHEEL: {
            return 0;
        }

        case WM_SIZE:{
            const int width = LOWORD(lParam);
            const int height = HIWORD(lParam);

            if (width != window->width || height != window->height){
                window->width = width;
                window->height = height;

                List_ForAllItems(&window->onSizeChangeCall, [=](OnSizeChangeCallback* sc){
                    sc->fn(window->width, window->height, sc->priv);
                });
            }

            return 0;
        }

        case WM_ERASEBKGND:{
            return TRUE;
        }

        case WM_DROPFILES:{
            return 0;
        }
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

bool RegisterWindowClassWin32(){
    WNDCLASSEXW wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = win32Helper.instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = DISPLAY_CLASS_NAME;

    wc.hIcon = (HICON)LoadImageW(NULL, IDI_APPLICATION, IMAGE_ICON,
                           0, 0, LR_DEFAULTSIZE | LR_SHARED);


    if (!RegisterClassExW(&wc)){
        ERROR_MSG("Win32: Failed to register window class");
        return false;
    }

    return true;
}

void InitializeWin32() {
    STARTED();
    memset(&win32Helper, 0, sizeof(LibHelperWin32));

    SystemParametersInfoW(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &win32Helper.foregroundLockTimeout, 0);
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, UIntToPtr(0), SPIF_SENDCHANGE);

    if (!loadLibraries()) {
        ERROR_MSG("Could not load libraries\n");
        return;
    }

    if (IsWindows10CreatorsUpdateOrGreaterWin32())
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (IsWindows8Point1OrGreater())
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    else if (IsWindowsVistaOrGreater())
        SetProcessDPIAware();

    if (!RegisterWindowClassWin32()) {
        ERROR_MSG("Could not create register class");
        return;
    }

    if (!createHelperWindow()) {
        ERROR_MSG("Could not create helper window");
        return;
    }

    // TODO: Does this really need to be here?
    InitializeSystemTimer();
    // _glfwInitJoysticksWin32
    // _glfwPollMonitorsWin32

    InitializeFramebuffer(&win32Framebuffer);
    win32GLContext.forward = 1;
    win32GLContext.profile = OPENGL_CORE_PROFILE;

    FINISHED();
}

int WindowShouldCloseWin32(WindowWin32* window) {
    return window->shouldClose;
}

void SwapIntervalWin32(WindowWin32* window, int interval){
    window->wgl.swapInterval(window, interval);
}

int WindowIsMousePressedWin32(WindowWin32 *window){
    return window->isMousePressed;
}

void SetWindowShouldCloseWin32(WindowWin32* window) {
    window->shouldClose = 1;
}

static DWORD getWindowExStyle(const WindowWin32* window)
{
    DWORD style = WS_EX_APPWINDOW;
#if 0
    if (window->monitor || window->floating)
        style |= WS_EX_TOPMOST;
#endif
    return style;
}

static DWORD getWindowStyle(const WindowWin32* window)
{
    return WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX |
           WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION;
}

WCHAR* CreateWideStringFromUTF8Win32(const char* source){
    WCHAR* target;
    int count;

    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    if (!count)
    {
        ERROR_MSG("Win32: Failed to convert string from UTF-8");
        return NULL;
    }

    target = (WCHAR *)calloc(count, sizeof(WCHAR));

    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, target, count))
    {
        ERROR_MSG("Win32: Failed to convert string from UTF-8");
        free(target);
        return NULL;
    }

    return target;
}


static int createNativeWindow(int width, int height, const char* title,
                              WindowWin32 *window)
{
    int xpos = 0, ypos = 0;
    WCHAR* wideTitle;
    DWORD style = getWindowStyle(window);
    DWORD exStyle = getWindowExStyle(window);

    wideTitle = CreateWideStringFromUTF8Win32(title);
    if (!wideTitle)
        return false;

    RECT windowRect = { 0, 0, width, height };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    width = windowRect.right - windowRect.left;
    height = windowRect.bottom - windowRect.top;

    HWND foregroundWindow = GetForegroundWindow();
    HMONITOR monitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (foregroundWindow) {
        monitor = MonitorFromWindow(foregroundWindow, MONITOR_DEFAULTTOPRIMARY);
    }

    if (monitor) {
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(monitor, &monitorInfo)) {
            xpos = monitorInfo.rcWork.left +
                (monitorInfo.rcWork.right - monitorInfo.rcWork.left - width) / 2;
            ypos = monitorInfo.rcWork.top +
                (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - height) / 2;
        }
    }

    window->handle = CreateWindowExW(exStyle,
        DISPLAY_CLASS_NAME,
        wideTitle,
        style,
        xpos, ypos,
        width, height,
        NULL, // No parent window
        NULL, // No window menu
        win32Helper.instance,
        NULL);

    free(wideTitle);

    if (!window->handle){
        ERROR_MSG("Win32: Failed to create window");
        log_last_error();
        return false;
    }

    SetPropW(window->handle, DISPLAY_CLASS_NAME, window);

    if (IsWindows7OrGreater()){
        ChangeWindowMessageFilterEx(window->handle, WM_DROPFILES, MSGFLT_ALLOW, NULL);
        ChangeWindowMessageFilterEx(window->handle, WM_COPYDATA, MSGFLT_ALLOW, NULL);
        ChangeWindowMessageFilterEx(window->handle, WM_COPYGLOBALDATA, MSGFLT_ALLOW, NULL);
    }

    return true;
}

void SetOpenGLVersionWin32(int major, int minor) {
    win32GLContext.major = major;
    win32GLContext.minor = minor;
}

void PollEventsWin32()
{
    MSG msg;
    HWND handle;
    WindowWin32* window = nullptr;
    handle = GetActiveWindow();

    if (handle)
        window = (WindowWin32*)GetPropW(handle, DISPLAY_CLASS_NAME);

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)){
        if (msg.message == WM_QUIT){
            // NOTE: While GLFW does not itself post WM_QUIT, other processes
            //       may post it to this one, for example Task Manager
            // HACK: Treat WM_QUIT as a close on all windows
            if (window){
                window->shouldClose = 1;
            }
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // HACK: Release modifier keys that the system did not emit KEYUP for
    // NOTE: Shift keys on Windows tend to "stick" when both are pressed as
    //       no key up message is generated by the first key release
    // NOTE: Windows key is not reported as released by the Win+V hotkey
    //       Other Win hotkeys are handled implicitly by _glfwInputWindowFocus
    //       because they change the input focus
    // NOTE: The other half of this is in the WM_*KEY* handler in windowProc
    if (handle){
        if (window){
            int i;
            const int keys[2][2] =
            {
                { VK_LSHIFT, Key_LeftShift },
                { VK_RSHIFT, Key_RightShift },
            };
#if 0
            for (i = 0; i < 2; i++)
            {
                const int vk = keys[i][0];
                const int key = keys[i][1];
                const int scancode = _glfw.win32.scancodes[key];

                if ((GetKeyState(vk) & 0x8000))
                    continue;
                if (window->keys[key] != GLFW_PRESS)
                    continue;

                _glfwInputKey(window, key, scancode, GLFW_RELEASE, getKeyMods());
            }
#endif
        }
        else {
            ERROR_MSG("GOT NO WINDOW PTR");
        }
    }
#if 0
    window = win32Helper.disabledCursorWindow;
    if (window)
    {
        int width, height;
        _glfwPlatformGetWindowSize(window, &width, &height);

        // NOTE: Re-center the cursor only if it has moved since the last call,
        //       to avoid breaking glfwWaitEvents with WM_MOUSEMOVE
        if (window->win32.lastCursorPosX != width / 2 ||
            window->win32.lastCursorPosY != height / 2)
        {
            _glfwPlatformSetCursorPos(window, width / 2, height / 2);
        }
    }
#endif
}

bool CreateContextWGL(WindowWin32* window, const ContextGL* ctxconfig, const Framebuffer* fbconfig);

void WaitEventsWin32(){
    WaitMessage();
    PollEventsWin32();
}

void MakeContextWin32(WindowWin32* window){
    MakeContextWGL(window);
}

void SwapBuffersWin32(WindowWin32* window){
    SwapBuffersWGL(window);
}

WindowWin32* CreateWindowWin32(int width, int height, const char* title) {
    STARTED();
    WindowWin32* window = AllocatorGetN(WindowWin32, 1);

    if (!window) {
        ERROR_MSG("Could not allocate window");
        return nullptr;
    }

    if(!createNativeWindow(width, height, title, window))
        return nullptr;

    if(!InitWGL())
        return nullptr;

    if (!CreateContextWGL(window, &win32GLContext, &win32Framebuffer))
        return nullptr;

    ShowWindow(window->handle, SW_SHOWNA);
    BringWindowToTop(window->handle);
    SetForegroundWindow(window->handle);
    SetFocus(window->handle);

    MakeContextWGL(window);

    FINISHED();
    return window;
}

void UnregisterWindowClassWin32(){
    UnregisterClassW(DISPLAY_CLASS_NAME, win32Helper.instance);
}

void TerminateWin32(){
    if (win32Helper.deviceNotificationHandle)
        UnregisterDeviceNotification(win32Helper.deviceNotificationHandle);

    if (win32Helper.helperWindowHandle)
        DestroyWindow(win32Helper.helperWindowHandle);

    UnregisterWindowClassWin32();

    // Restore previous foreground lock timeout system setting
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0,
        UIntToPtr(win32Helper.foregroundLockTimeout), SPIF_SENDCHANGE);

    free(win32Helper.clipboardString);
    free(win32Helper.rawInput);

    TerminateWGL();

    //freeLibraries();
}

void DestroyWindowWin32(WindowWin32* window){
    RemovePropW(window->handle, DISPLAY_CLASS_NAME);
    DestroyWindow(window->handle);
}

void SetWindowTitleWin32(WindowWin32* window, const char* title){
    WCHAR* wideTitle = CreateWideStringFromUTF8Win32(title);
    if (!wideTitle)
        return;

    SetWindowTextW(window->handle, wideTitle);
    free(wideTitle);
}

void GetLastRecordedMousePositionWin32(WindowWin32* window, int* x, int* y){
    if (window) {
        if (x) *x = window->lastCursorPosX;
        if (y) *y = window->lastCursorPosY;
    }
}

uint RegisterOnScrollCallback(WindowWin32* window, onScrollCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onScrollCall, OnScrollCallback, EventMaskScroll);
}

uint RegisterOnMouseLeftClickCallback(WindowWin32* window, onMouseLClickCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMouseLClickCall, OnMouseLClickCallback, EventMaskLeftClick);
}

uint RegisterOnMouseRightClickCallback(WindowWin32* window, onMouseRClickCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMouseRClickCall, OnMouseRClickCallback, EventMaskRightClick);
}

uint RegisterOnMousePressedCallback(WindowWin32* window, onMousePressedCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMousePressedCall, OnMousePressedCallback, EventMaskPress);
}

uint RegisterOnMouseReleasedCallback(WindowWin32* window, onMouseReleasedCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMouseReleasedCall, OnMouseReleasedCallback, EventMaskRelease);
}

uint RegisterOnSizeChangeCallback(WindowWin32* window, onSizeChangeCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onSizeChangeCall, OnSizeChangeCallback, EventMaskSizeChange);
}

uint RegisterOnFocusChangeCallback(WindowWin32* window, onFocusChangeCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onFocusChangeCall, OnFocusChangeCallback, EventMaskFocusChange);
}

uint RegisterOnMouseMotionCallback(WindowWin32* window, onMouseMotionCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMouseMotionCall, OnMouseMotionCallback, EventMaskMotion);
}

uint RegisterOnMouseDoubleClickCallback(WindowWin32* window, onMouseDClickCallback* callback, void* priv) {
    RegisterCallback(window, callback, priv, onMouseDClickCall, OnMouseDClickCallback, EventMaskDoubleClick);
}

void UnregisterCallbackByHandle(WindowWin32* wnd, uint hnd) {
    uint bits = hnd >> 8;
    switch (bits) {
        case EventMaskScroll: {
            UnregisterCallback(wnd, hnd, onScrollCall, OnScrollCallback);
        } break;
        case EventMaskLeftClick: {
            UnregisterCallback(wnd, hnd, onMouseLClickCall, OnMouseLClickCallback);
        } break;
        case EventMaskRightClick: {
            UnregisterCallback(wnd, hnd, onMouseRClickCall, OnMouseRClickCallback);
        } break;
        case EventMaskPress: {
            UnregisterCallback(wnd, hnd, onMousePressedCall, OnMousePressedCallback);
        } break;
        case EventMaskRelease: {
            UnregisterCallback(wnd, hnd, onMouseReleasedCall, OnMouseReleasedCallback);
        } break;
        case EventMaskSizeChange: {
            UnregisterCallback(wnd, hnd, onSizeChangeCall, OnSizeChangeCallback);
        } break;
        case EventMaskFocusChange: {
            UnregisterCallback(wnd, hnd, onFocusChangeCall, OnFocusChangeCallback);
        } break;
        case EventMaskMotion: {
            UnregisterCallback(wnd, hnd, onMouseMotionCall, OnMouseMotionCallback);
        } break;
        case EventMaskDoubleClick: {
            UnregisterCallback(wnd, hnd, onMouseDClickCall, OnMouseDClickCallback);
        } break;
        default: {
            printf("Unkown mask value %u\n", bits);
        }
    }
}

static void InitWindowCallbackListX11(WindowWin32* window) {
    List_Init(&window->onScrollCall);
    List_Init(&window->onMouseLClickCall);
    List_Init(&window->onMouseRClickCall);
    List_Init(&window->onMousePressedCall);
    List_Init(&window->onMouseDClickCall);
    List_Init(&window->onMouseMotionCall);
    List_Init(&window->onSizeChangeCall);
    List_Init(&window->onFocusChangeCall);
}

static void ClearWindowCallbackListX11(WindowWin32* window) {
    List_Clear(&window->onScrollCall);
    List_Clear(&window->onMouseLClickCall);
    List_Clear(&window->onMouseRClickCall);
    List_Clear(&window->onMousePressedCall);
    List_Clear(&window->onMouseDClickCall);
    List_Clear(&window->onMouseMotionCall);
    List_Clear(&window->onSizeChangeCall);
    List_Clear(&window->onFocusChangeCall);
}
