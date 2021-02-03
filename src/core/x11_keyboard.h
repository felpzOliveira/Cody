/* date = February 2nd 2021 5:56 pm */

#ifndef X11_KEYBOARD_H
#define X11_KEYBOARD_H

#include <x11_display.h>
#include <keyboard.h>

int CreateKeyTableX11();
int TranslateKeyX11(int scancode);
int GetMappedKeysCountX11();

#endif //X11_KEYBOARD_H
