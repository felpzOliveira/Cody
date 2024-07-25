#pragma once
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef VC_EXTRALEAN
    #define VC_EXTRALEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
    #define UNICODE
#endif

#include <wctype.h>
#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#include <dbt.h>
#include <stdint.h>
#include <wgl_helper.h>
#include <utilities.h>
#include <types.h>

#define OPENGL_COMPAT_PROFILE 0
#define OPENGL_CORE_PROFILE   1

#define MOUSE_DCLICK_INTERVAL 300 // ms
#define MOUSE_DCLICK_DISTANCE 10 // pixels

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

#ifndef WM_COPYGLOBALDATA
    #define WM_COPYGLOBALDATA 0x0049
#endif

#if WINVER < 0x0601
typedef struct
{
    DWORD cbSize;
    DWORD ExtStatus;
} CHANGEFILTERSTRUCT;
#ifndef MSGFLT_ALLOW
#define MSGFLT_ALLOW 1
#endif
#endif /*Windows 7*/

#if WINVER < 0x0600
#define DWM_BB_ENABLE 0x00000001
#define DWM_BB_BLURREGION 0x00000002
typedef struct
{
    DWORD dwFlags;
    BOOL fEnable;
    HRGN hRgnBlur;
    BOOL fTransitionOnMaximized;
} DWM_BLURBEHIND;
#else
#include <dwmapi.h>
#endif /*Windows Vista*/

#ifndef DPI_ENUMS_DECLARED
typedef enum
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum
{
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif /*DPI_ENUMS_DECLARED*/

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) -4)
#endif /*DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2*/

typedef DWORD(WINAPI* PFN_XInputGetCapabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, XINPUT_STATE*);
#define XInputGetCapabilities win32Helper.xinput.GetCapabilities
#define XInputGetState win32Helper.xinput.GetState

// dinput8.dll function pointer typedefs
typedef HRESULT(WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
#define DirectInput8Create win32Helper.dinput8.Create

// user32.dll function pointer typedefs
typedef BOOL(WINAPI* PFN_SetProcessDPIAware)(void);
typedef BOOL(WINAPI* PFN_ChangeWindowMessageFilterEx)(HWND, UINT, DWORD, CHANGEFILTERSTRUCT*);
typedef BOOL(WINAPI* PFN_EnableNonClientDpiScaling)(HWND);
typedef BOOL(WINAPI* PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
typedef BOOL(WINAPI* PFN_AdjustWindowRectExForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
typedef int (WINAPI* PFN_GetSystemMetricsForDpi)(int, UINT);
#define SetProcessDPIAware win32Helper.user32.SetProcessDPIAware_
#define ChangeWindowMessageFilterEx win32Helper.user32.ChangeWindowMessageFilterEx_
#define EnableNonClientDpiScaling win32Helper.user32.EnableNonClientDpiScaling_
#define SetProcessDpiAwarenessContext win32Helper.user32.SetProcessDpiAwarenessContext_
#define GetDpiForWindow win32Helper.user32.GetDpiForWindow_
#define AdjustWindowRectExForDpi win32Helper.user32.AdjustWindowRectExForDpi_
#define GetSystemMetricsForDpi win32Helper.user32.GetSystemMetricsForDpi_

// dwmapi.dll function pointer typedefs
typedef HRESULT(WINAPI* PFN_DwmIsCompositionEnabled)(BOOL*);
typedef HRESULT(WINAPI* PFN_DwmFlush)(VOID);
typedef HRESULT(WINAPI* PFN_DwmEnableBlurBehindWindow)(HWND, const DWM_BLURBEHIND*);
typedef HRESULT(WINAPI* PFN_DwmGetColorizationColor)(DWORD*, BOOL*);
#define DwmIsCompositionEnabled win32Helper.dwmapi.IsCompositionEnabled
#define DwmFlush win32Helper.dwmapi.Flush
#define DwmEnableBlurBehindWindow win32Helper.dwmapi.EnableBlurBehindWindow
#define DwmGetColorizationColor win32Helper.dwmapi.GetColorizationColor

// shcore.dll function pointer typedefs
typedef HRESULT(WINAPI* PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
typedef HRESULT(WINAPI* PFN_GetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
#define SetProcessDpiAwareness win32Helper.shcore.SetProcessDpiAwareness_
#define GetDpiForMonitor win32Helper.shcore.GetDpiForMonitor_

// ntdll.dll function pointer typedefs
typedef LONG(WINAPI* PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW*, ULONG, ULONGLONG);
#define RtlVerifyVersionInfo win32Helper.ntdll.RtlVerifyVersionInfo_

#define IsWindowsXPOrGreater()                                 \
    IsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WINXP),   \
                                        LOBYTE(_WIN32_WINNT_WINXP), 0)
#define IsWindowsVistaOrGreater()                                     \
    IsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_VISTA),   \
                                        LOBYTE(_WIN32_WINNT_VISTA), 0)
#define IsWindows7OrGreater()                                         \
    IsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WIN7),    \
                                        LOBYTE(_WIN32_WINNT_WIN7), 0)
#define IsWindows8OrGreater()                                         \
    IsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WIN8),    \
                                        LOBYTE(_WIN32_WINNT_WIN8), 0)
#define IsWindows8Point1OrGreater()                                   \
    IsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WINBLUE), \
                                        LOBYTE(_WIN32_WINNT_WINBLUE), 0)

#define IsWindows10AnniversaryUpdateOrGreaterWin32() \
    IsWindows10BuildOrGreaterWin32(14393)
#define IsWindows10CreatorsUpdateOrGreaterWin32() \
    IsWindows10BuildOrGreaterWin32(15063)

#define ON_SCROLL_CALLBACK(name) void name(int is_up, void *priv)
#define ON_MOUSE_LCLICK_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_RCLICK_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_PRESS_CALLBACK(name) void name(int x, int y, void *priv)
#define ON_MOUSE_RELEASE_CALLBACK(name) void name(int x, int y, void *priv)
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
struct CallbackWin32 {
    Fn* fn;
    void* priv;
    uint handle;
};

typedef CallbackWin32<onScrollCallback> OnScrollCallback;
typedef CallbackWin32<onMouseLClickCallback> OnMouseLClickCallback;
typedef CallbackWin32<onMouseRClickCallback> OnMouseRClickCallback;
typedef CallbackWin32<onMousePressedCallback> OnMousePressedCallback;
typedef CallbackWin32<onMouseReleasedCallback> OnMouseReleasedCallback;
typedef CallbackWin32<onMouseDClickCallback> OnMouseDClickCallback;
typedef CallbackWin32<onMouseMotionCallback> OnMouseMotionCallback;
typedef CallbackWin32<onSizeChangeCallback> OnSizeChangeCallback;
typedef CallbackWin32<onFocusChangeCallback> OnFocusChangeCallback;
typedef CallbackWin32<onDragAndDropCallback> OnDragAndDropCallback;

typedef struct {
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

typedef struct {
    int major;
    int minor;
    int forward;
    int profile;
    void* window;
    void* share;
}ContextGL;

typedef struct WindowWin32{
	HWND handle;
    int lastCursorPosX, lastCursorPosY;
    WCHAR highSurrogate;
    int isMousePressed;
    int shouldClose;
    int width, height;
    int xpos, ypos;
    DWORD lastKeyTime;
    int lastKeyScancode;
    DWORD lastClickTime;
    contextWGL wgl;
    void *binding;
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
}WindowWin32;

typedef struct {
    HINSTANCE instance;
    DWORD foregroundLockTimeout;
    HWND helperWindowHandle;
    HDEVNOTIFY deviceNotificationHandle;
    RAWINPUT *rawInput;
    char* clipboardString;
    int rawInputSize;
    uint64_t frequency;

    struct {
        HINSTANCE                       instance;
        PFN_DirectInput8Create          Create;
        IDirectInput8W* api;
    } dinput8;

    struct {
        HINSTANCE                       instance;
        PFN_XInputGetCapabilities       GetCapabilities;
        PFN_XInputGetState              GetState;
    } xinput;

    struct {
        HINSTANCE                       instance;
        PFN_SetProcessDPIAware          SetProcessDPIAware_;
        PFN_ChangeWindowMessageFilterEx ChangeWindowMessageFilterEx_;
        PFN_EnableNonClientDpiScaling   EnableNonClientDpiScaling_;
        PFN_SetProcessDpiAwarenessContext SetProcessDpiAwarenessContext_;
        PFN_GetDpiForWindow             GetDpiForWindow_;
        PFN_AdjustWindowRectExForDpi    AdjustWindowRectExForDpi_;
        PFN_GetSystemMetricsForDpi      GetSystemMetricsForDpi_;
    } user32;

    struct {
        HINSTANCE                       instance;
        PFN_DwmIsCompositionEnabled     IsCompositionEnabled;
        PFN_DwmFlush                    Flush;
        PFN_DwmEnableBlurBehindWindow   EnableBlurBehindWindow;
        PFN_DwmGetColorizationColor     GetColorizationColor;
    } dwmapi;

    struct {
        HINSTANCE                       instance;
        PFN_SetProcessDpiAwareness      SetProcessDpiAwareness_;
        PFN_GetDpiForMonitor            GetDpiForMonitor_;
    } shcore;

    struct {
        HINSTANCE                       instance;
        PFN_RtlVerifyVersionInfo        RtlVerifyVersionInfo_;
    } ntdll;
}LibHelperWin32;

void InitializeWin32();

WindowWin32* CreateWindowWin32(int width, int height, const char* title);
void DestroyWindowWin32(WindowWin32* window);
void SetWindowTitleWin32(WindowWin32* window, const char* title);
void TerminateWin32();
void WaitEventsWin32();
void PollEventsWin32();

void SetWindowIconWin32(WindowWin32 *window, unsigned char *png, unsigned int pngLen);
int WindowShouldCloseWin32(WindowWin32* window);
int WindowIsMousePressedWin32(WindowWin32 *window);
void SetWindowShouldCloseWin32(WindowWin32* window);
void SetOpenGLVersionWin32(int major, int minor);

void MakeContextWin32(WindowWin32* window);
void SwapBuffersWin32(WindowWin32* window);
void SwapIntervalWin32(WindowWin32* window, int interval);
void SetClipboardStringWin32(const char* string);
const char* GetClipboardStringWin32(int *size);

void GetLastRecordedMousePositionWin32(WindowWin32* window, int* x, int* y);

uint RegisterOnScrollCallback(WindowWin32* window, onScrollCallback* callback, void* priv);
uint RegisterOnMousePressedCallback(WindowWin32* window, onMousePressedCallback* callback, void* priv);
uint RegisterOnMouseReleasedCallback(WindowWin32* window, onMouseReleasedCallback* callback, void* priv);
uint RegisterOnMouseLeftClickCallback(WindowWin32* window, onMouseLClickCallback* callback, void* priv);
uint RegisterOnMouseRightClickCallback(WindowWin32* window, onMouseRClickCallback* callback, void* priv);
uint RegisterOnMouseDoubleClickCallback(WindowWin32* window, onMouseDClickCallback* callback, void* priv);
uint RegisterOnSizeChangeCallback(WindowWin32* window, onSizeChangeCallback* callback, void* priv);
uint RegisterOnFocusChangeCallback(WindowWin32* window, onFocusChangeCallback* callback, void* priv);
uint RegisterOnMouseMotionCallback(WindowWin32* window, onMouseMotionCallback* callback, void* priv);
uint RegisterOnDragAndDropCallback(WindowWin32* window, onDragAndDropCallback* callback, void* priv);

void UnregisterCallbackByHandle(WindowWin32* window, uint handle);
