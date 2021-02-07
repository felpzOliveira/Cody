/* date = February 2nd 2021 5:37 pm */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEYBOARD_EVENT_RELEASE 0
#define KEYBOARD_EVENT_PRESS   1
#define KEYBOARD_EVENT_REPEAT  2

#define KEYSET_MAX_SIZE 6
#define MAX_BINDINGS_PER_ENTRY 128
#define KEYSET_EVENT(name) void name()
#define KEY_ENTRY_EVENT(name) void name(char *utf8Data, int utf8Size)

typedef KEYSET_EVENT(KeySetEventCallback);
typedef KEY_ENTRY_EVENT(KeyEntryCallback);

#define RegisterEvent(map, callback, ...) RegisterKeyboardEventEx(map, 0, callback, __VA_ARGS__, -1)

#define RegisterRepeatableEvent(map, callback, ...) RegisterKeyboardEventEx(map, 1, callback, __VA_ARGS__, -1)

typedef enum{
    Key_A = 1,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_X,
    Key_W,
    Key_Y,
    Key_Z,
    Key_0,
    Key_1,
    Key_2,
    Key_3,
    Key_4,
    Key_5,
    Key_6,
    Key_7,
    Key_8,
    Key_9,
    Key_Comma,
    Key_Period,
    Key_Slash,
    Key_Semicolon,
    Key_Equal,
    Key_Minus,
    Key_LeftBracket,
    Key_Backslash,
    Key_RightBracket,
    Key_Escape,
    Key_Tab,
    Key_Space,
    Key_Backspace,
    Key_Insert,
    Key_Delete,
    Key_Right,
    Key_Left,
    Key_Down,
    Key_Up,
    Key_PageUp,
    Key_PageDown,
    Key_Home,
    Key_End,
    Key_LeftShift,
    Key_LeftControl,
    Key_LeftAlt,
    Key_RightShift,
    Key_RightControl,
    Key_RightAlt,
    Key_Enter,
    Key_Unmapped
}Key;

typedef struct{
    Key keySet[KEYSET_MAX_SIZE];
    KeySetEventCallback *callback;
    int keySetSize;
    int supportsRepeat;
}Binding;

typedef struct{
    Binding *bindings;
    int *bindingCount;
    KeyEntryCallback *entryCallback;
}BindingMap;

void KeyboardInitMappings();
const char *KeyboardGetKeyName(Key key);
const char *KeyboardGetStateName(int state);

BindingMap *KeyboardCreateMapping();
void KeyboardSetActiveMapping(BindingMap *mapping);

void RegisterKeyboardDefaultEntry(BindingMap *mapping, KeyEntryCallback *callback);
void RegisterKeyboardEventEx(BindingMap *mapping, int repeat,
                             KeySetEventCallback *callback, ...);

void KeyboardEvent(Key eventKey, int eventType, char *utf8Data, int utf8Size,
                   int rawKeyCode);
void KeyboardResetState();

#endif //KEYBOARD_H
