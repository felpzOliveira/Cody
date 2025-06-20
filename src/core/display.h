#pragma once
#include <types.h>

#if defined(_WIN32)
    #include <win_display.h>
    #define DisplayWindow WindowWin32
#else
    #include <x11_display.h>
    #define DisplayWindow WindowX11
#endif

DisplayWindow *CreateDisplayWindow(int width, int height, const char *title);

void ClipboardSetString(char *ptr, uint size);

void TerminateDisplay();

const char *ClipboardGetString(uint *size);

void DestroyWindow(DisplayWindow *window);

void SwapInterval(DisplayWindow *window, int interval);

void SetWindowIcon(DisplayWindow *window, unsigned char *png, unsigned int pngLen);

void PoolEvents();

void WaitForEvents();

void MakeContextCurrent(DisplayWindow *window);

int WindowShouldClose(DisplayWindow *window);

void SetWindowShouldClose(DisplayWindow *window);

void SwapBuffers(DisplayWindow *window);

void GetLastRecordedMousePosition(DisplayWindow *window, int *x, int *y);

void InitializeDisplay();

void SetSamples(int samples);

void SetOpenGLVersion(int major, int minor);

void ShowCursor(DisplayWindow *window);

void HideCursor(DisplayWindow *window);

int IsCursorAlive();
