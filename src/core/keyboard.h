/* date = February 2nd 2021 5:37 pm */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <types.h>

#define KEYBOARD_EVENT_RELEASE 0
#define KEYBOARD_EVENT_PRESS   1
#define KEYBOARD_EVENT_REPEAT  2

#define KEYSET_MAX_SIZE 4
#define MAX_BINDINGS_PER_ENTRY 48
#define KEYSET_EVENT(name) void name()
#define KEY_ENTRY_EVENT(name) void name(char *utf8Data, int utf8Size, void *priv)
#define KEY_ENTRY_RAW_EVENT(name) void name(int rawKeyCode)

#define KEYBOARD_ACTIVE_EVENT(name) void name()

typedef KEYSET_EVENT(KeySetEventCallback);
typedef KEY_ENTRY_EVENT(KeyEntryCallback);
typedef KEY_ENTRY_RAW_EVENT(KeyRawEntryCallback);
typedef KEYBOARD_ACTIVE_EVENT(KeyboardActiveCallback);

#define RegisterEvent(map, callback, ...) RegisterKeyboardEventEx(map, 0, #callback, callback, __VA_ARGS__, -1)

#define RegisterRepeatableEvent(map, callback, ...) RegisterKeyboardEventEx(map, 1, #callback, callback, __VA_ARGS__, -1)

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
    Key_F1,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_F11,
    Key_F12,
    Key_Comma,
    Key_Period,
    Key_Slash,
    Key_Semicolon,
    Key_Equal,
    Key_Minus,
    Key_Plus,
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
    char bindingname[128];
}Binding;

typedef struct{
    Binding *bindings;
    int *bindingCount;
    KeyEntryCallback *entryCallback;
    KeyRawEntryCallback *entryRawCallback;
    void *refWindow;
    void *userPriv;
}BindingMap;

/*
* Initializes the keyboard API.
*/
void KeyboardInitMappings();

/*
* Get readable name for keys.
*/
const char *KeyboardGetKeyName(Key key);
const char *KeyboardGetStateName(int state);

/*
* Creates a new keyboard mapping to handle key events. Optionally
* attach it to a specific window.
*/
BindingMap *KeyboardCreateMapping();
BindingMap *KeyboardCreateMapping(void *refWindow);


#define _KeyboardSetActiveMapping(mapping)do{\
    printf("Called from %s:%d [%s]\n", __FILE__, __LINE__, __func__);\
    KeyboardSetActiveMapping(mapping);\
}while(0)
/*
* Sets the active mapping to handle key events.
*/
void KeyboardSetActiveMapping(BindingMap *mapping);

/*
* Register a callback to be called everytime the keyboard is active,
* do not do heavy work in here please.
*/
int KeyboardRegisterActiveEvent(KeyboardActiveCallback *cb);

/*
* Remove a callback from the list of active callbacks for the keyboard.
*/
void KeyboardReleaseActiveEvent(int handle);

/*
* Sets the window where the mapping should be applied. If a keyboard event
* happens and the active binding does not have a reference window than
* it is applied globally. Use this routine to tell cody to only call
* this bindings when they happen on a given target window.
*/
void KeyboardSetReferenceWindow(BindingMap *mapping, void *window);

/*
* Queries the keyboard API for the current active mapping.
*/
BindingMap *KeyboardGetActiveMapping();

/*
* Get the state of a given key _right_now_.
*/
int KeyboardGetKeyState(Key key);
int KeyboardGetKeyActive(Key key);

/*
* Queries the keyboard API for the current active mapping for a specific window.
*/
BindingMap *KeyboardGetActiveMapping(void *window);

/*
* Register default callback for key entry values for unbinded UTF-8 strings.
*/
void RegisterKeyboardDefaultEntry(BindingMap *mapping, KeyEntryCallback *callback,
                                  void *userData);

/*
* Register a raw key event callback. This function (when binded) is called
* whenever a key event happens and it was not consumed by the current binding,
* note however that this passes raw events and does not process keys.
*/
void RegisterKeyboardRawEntry(BindingMap *mapping, KeyRawEntryCallback *callback);

/*
* Register a key combination to a specific callback. Do not call directly, use
* the macro 'RegisterRepeatableEvent' or 'RegisterEvent' for easier setup.
*/
void RegisterKeyboardEventEx(BindingMap *mapping, int repeat, const char *name,
                             KeySetEventCallback *callback, ...);

/*
* Reports a key event. This should be called by the underlying OS keyboard system.
*/
void KeyboardEvent(Key eventKey, int eventType, char *utf8Data, int utf8Size,
                   int rawKeyCode, void *window, int allowEntryCallback);
/*
* Dedicated event reporting for INPUT text only, this routine does not trigger events registered with
* RegisterEvent*, it is meant for windows so it can split its message.
*/
void KeyboardEntryEvent(Key eventKey, int eventType, char* utf8Data, int utf8Size,
                        int rawKeyCode, void* window);

/*
* Forces the keyboard API table to restore all key states, no callbacks are triggered.
*/
void KeyboardResetState();

int IsKeyModifier(Key key);

int IsKeySpecialBinding(Key key);

void KeyboardRegisterKeyState(Key eventKey, int eventType);

int KeyboardAttemptToConsumeKey(Key eventKey, void *window, Binding **target);

int KeyboardIsKeyInBinding(Key eventKey, Binding *target);

// NOTE: These should be defined somewhere else
int CreateKeyTable();
int TranslateKey(int scancode);
int GetMappedKeysCount();

#endif //KEYBOARD_H
