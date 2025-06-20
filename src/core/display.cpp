#include <display.h>

#if defined(_WIN32)
#include <win_display.h>
DisplayWindow *CreateDisplayWindow(int width, int height, const char *title){
    return CreateWindowWin32(width, height, title);
}

void SetWindowIcon(DisplayWindow *window, unsigned char *png, unsigned int pngLen){
    SetWindowIconWin32(window, png, pngLen);
}

void SetOpenGLVersion(int major, int minor){
    SetOpenGLVersionWin32(major, minor);
}

void SetSamples(int samples){
    //SetSamplesWin32(samples);
}

void InitializeDisplay(){
    InitializeWin32();
}

void ClipboardSetString(char *ptr, uint size){
    SetClipboardStringWin32(ptr);
}

const char *ClipboardGetString(uint *size){
    return GetClipboardStringWin32((int *)size);
}

void DestroyWindow(DisplayWindow *window){
    DestroyWindowWin32(window);
}

void SetWindowShouldClose(DisplayWindow *window){
    SetWindowShouldCloseWin32(window);
}

void SwapInterval(DisplayWindow *window, int interval){
    SwapIntervalWin32(window, interval);
}

void MakeContextCurrent(DisplayWindow *window){
    MakeContextWin32(window);
}

int WindowShouldClose(DisplayWindow *window){
    return WindowShouldCloseWin32(window);
}

void SwapBuffers(DisplayWindow *window){
    SwapBuffersWin32(window);
}

void GetLastRecordedMousePosition(DisplayWindow *window, int *x, int *y){
    GetLastRecordedMousePositionWin32(window, x, y);
}

void ShowCursor(DisplayWindow *window){
    ShowCursorWin32(window);
}

void HideCursor(DisplayWindow *window){
    HideCursorWin32(window);
}

int IsCursorAlive(){
    return IsCursorDisplayedWin32();
}

void PoolEvents(){
    PollEventsWin32();
}

void WaitForEvents(){
    WaitEventsWin32();
}

void TerminateDisplay(){
    TerminateWin32();
}
#else
#include <x11_display.h>
DisplayWindow *CreateDisplayWindow(int width, int height, const char *title){
    return CreateWindowX11(width, height, title);
}

void SetWindowIcon(DisplayWindow *window, unsigned char *png, unsigned int pngLen){
    SetWindowIconX11(window, png, pngLen);
}

void SetOpenGLVersion(int major, int minor){
    SetOpenGLVersionX11(major, minor);
}

void SetSamples(int samples){
    SetSamplesX11(samples);
}

void InitializeDisplay(){
    InitializeX11();
}

void ClipboardSetString(char *ptr, uint size){
    ClipboardSetStringX11(ptr, size);
}

const char *ClipboardGetString(uint *size){
    return ClipboardGetStringX11(size);
}

void DestroyWindow(DisplayWindow *window){
    DestroyWindowX11(window);
}

void SwapInterval(DisplayWindow *window, int interval){
    SwapIntervalX11(window, interval);
}

void MakeContextCurrent(DisplayWindow *window){
    MakeContextX11(window);
}

int WindowShouldClose(DisplayWindow *window){
    return WindowShouldCloseX11(window);
}

void SwapBuffers(DisplayWindow *window){
    SwapBuffersX11(window);
}

void GetLastRecordedMousePosition(DisplayWindow *window, int *x, int *y){
    GetLastRecordedMousePositionX11(window, x, y);
}

void SetWindowShouldClose(DisplayWindow *window){
    SetWindowShouldCloseX11(window);
}

void ShowCursor(DisplayWindow *window){
    ShowCursorX11(window);
}

void HideCursor(DisplayWindow *window){
    HideCursorX11(window);
}

int IsCursorAlive(){
    return IsCursorDisplayedX11();
}

void PoolEvents(){
    PoolEventsX11();
}

void WaitForEvents(){
    WaitForEventsX11();
}

void TerminateDisplay(){
    TerminateX11();
}
#endif
