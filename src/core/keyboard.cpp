#include <keyboard.h>
#include <types.h>
#include <stdarg.h>
#include <utilities.h>
#include <unordered_map>
#include <string.h>

#define GetKeyID(key) ((int)key - (int)Key_A)
#define GetBindingID(ki, i) (ki * MAX_BINDINGS_PER_ENTRY + i)

static BindingMap *activeMapping = nullptr;
static int *keyStates;
static int unmappedKeyState = KEYBOARD_EVENT_RELEASE;
static std::unordered_map<void *, BindingMap *> activeWindowMapping;
static std::unordered_map<uint, KeyboardActiveCallback *> keyboardActiveCbMapping;

const char *KeyboardGetKeyName(Key key);
const char *KeyboardGetStateName(int state);

int KeyboardRegisterActiveEvent(KeyboardActiveCallback *cb){
    uint handle = Bad_RNG16();
    bool got_handle = true;
    do{
        got_handle = true;
        if(keyboardActiveCbMapping.find(handle) != keyboardActiveCbMapping.end()){
            got_handle = false;
            handle = Bad_RNG16();
        }
    }while(!got_handle);

    keyboardActiveCbMapping[handle] = cb;
    return (int)handle;
}

void KeyboardReleaseActiveEvent(int handle){
    keyboardActiveCbMapping.erase((uint)handle);
}

void KeyboardSetActiveMapping(BindingMap *mapping){
    if(mapping->refWindow){
        activeWindowMapping[mapping->refWindow] = mapping;
    }else{
        activeMapping = mapping;
    }
}

static BindingMap *KeyboardMappingForWindow(void *window){
    BindingMap *mapping = activeMapping;
    if(window){
        if(activeWindowMapping.find(window) != activeWindowMapping.end()){
            mapping = activeWindowMapping[window];
        }
    }

    return mapping;
}

BindingMap *KeyboardGetActiveMapping(){
    return activeMapping;
}

BindingMap *KeyboardGetActiveMapping(void *window){
    BindingMap *mapping = nullptr;
    if(window){
        if(activeWindowMapping.find(window) != activeWindowMapping.end()){
            mapping = activeWindowMapping[window];
        }
    }

    return mapping;
}

int KeyboardGetKeyState(Key key){
    return keyStates[GetKeyID(key)];
}

int KeyboardGetKeyActive(Key key){
    int state = KeyboardGetKeyState(key);
    return (state == KEYBOARD_EVENT_PRESS || state == KEYBOARD_EVENT_REPEAT) ? 1 : 0;
}

void KeyboardDebugPrintBindingKeys(Binding *binding){
    for(int i = 0; i < binding->keySetSize; i++){
        printf("%s ", KeyboardGetKeyName(binding->keySet[i]));
    }

    printf("\n");
}

void KeyboardSetReferenceWindow(BindingMap *mapping, void *window){
    if(mapping && window){
        mapping->refWindow = window;
    }
}

int KeyboardIsKeyInBinding(Key eventKey, Binding *binding){
    if(!binding)
        return 0;

    for(int k = 0; k < binding->keySetSize; k++){
        if(binding->keySet[k] == eventKey)
            return 1;
    }

    return 0;
}

int KeyboardProcessBindingEventsExactly(int keyHintId, BindingMap *mapping,
                                        Binding **target)
{
    int runned = 0;
    Binding *bestBinding = nullptr;
    int bestScore = -1;
    if(!mapping)
        return 0;

    int count = mapping->bindingCount[keyHintId];

    for(int i = 0; i < count; i++){
        int score = 0;
        Binding *bind = &mapping->bindings[GetBindingID(keyHintId, i)];
        for(int k = 0; k < bind->keySetSize; k++){
            int kState = keyStates[GetKeyID(bind->keySet[k])];
            if(kState == KEYBOARD_EVENT_RELEASE){
                score = -1;
                break;
            }

            if(kState == KEYBOARD_EVENT_PRESS || bind->supportsRepeat){
                score ++;
            }
        }

        if(score == bind->keySetSize){
            if(bestScore < score){
                bestScore = score;
                bestBinding = bind;
            }
        }
    }

    if(bestBinding){
        bestBinding->callback();
        *target = bestBinding;
        runned = 1;
    }

    return runned;
}

int KeyboardProcessBindingEvents(int keyId, BindingMap *mapping){
    Binding *bestBinding = nullptr;
    int runningScore = -1;
    int runned = 0;
    if(!mapping) return 0;

    int count = mapping->bindingCount[keyId];

    for(int i = 0; i < count; i++){
        int score = 0;
        Binding *bind = &mapping->bindings[GetBindingID(keyId, i)];
        for(int k = 0; k < bind->keySetSize; k++){
            int kState = keyStates[GetKeyID(bind->keySet[k])];
            if(kState == KEYBOARD_EVENT_RELEASE){
                score = -1;
                break;
            }

            if(kState == KEYBOARD_EVENT_PRESS || bind->supportsRepeat){
                score ++;
            }
        }

        if(score > runningScore){
            runningScore = score;
            bestBinding = bind;
        }
    }

    if(bestBinding){
        runned = 1;
        bestBinding->callback();
    }

    return runned;
}

void KeyboardEntryEvent(Key eventKey, int eventType, char* utf8Data, int utf8Size,
                        int rawKeyCode, void* window)
{
    int consumed = 0;
    int kid = GetKeyID(eventKey);
    int realEventType = eventType;
    BindingMap* mapping = KeyboardMappingForWindow(window);

    if (!mapping)
        return;

    if (eventKey != Key_Unmapped) {
        int currentKeyState = keyStates[kid];
        if (eventType == KEYBOARD_EVENT_PRESS) {
            if (currentKeyState != KEYBOARD_EVENT_RELEASE) {
                realEventType = KEYBOARD_EVENT_REPEAT; // TODO: Check if this is enough
            }
        }

        keyStates[kid] = realEventType;
    }
    else if (rawKeyCode != 0) {
        // I'm somehow getting a 0 code for UTF-8 that only happens once
        // i'm not finding any references to this so I'm skip it

        unmappedKeyState = eventType;
    }

    if (mapping && utf8Size > 0 && eventType != KEYBOARD_EVENT_RELEASE){
        if (mapping->entryCallback){
            mapping->entryCallback(utf8Data, utf8Size, mapping->userPriv);
            consumed = 1;
        }
    }

    if (mapping && consumed == 0) {
        if (mapping->entryRawCallback) {
            mapping->entryRawCallback(rawKeyCode);
        }
    }
}

void KeyboardRegisterKeyState(Key eventKey, int eventType){
    int kid = GetKeyID(eventKey);
    int realEventType = eventType;
    if(eventKey != Key_Unmapped){
        int currentKeyState = keyStates[kid];
        if(eventType == KEYBOARD_EVENT_PRESS){
            if(currentKeyState != KEYBOARD_EVENT_RELEASE){
                realEventType = KEYBOARD_EVENT_REPEAT; // TODO: Check if this is enough
            }
        }

        keyStates[kid] = realEventType;
    }
}

int KeyboardAttemptToConsumeKey(Key eventKey, void *window, Binding **target){
    int kid = GetKeyID(eventKey);
    BindingMap *mapping = KeyboardMappingForWindow(window);
    if(!mapping)
        return 0;

    if(eventKey != Key_Unmapped){
        return KeyboardProcessBindingEventsExactly(kid, mapping, target);
    }else
        return 0;
}

void KeyboardEvent(Key eventKey, int eventType, char *utf8Data, int utf8Size,
                   int rawKeyCode, void *window, int allowEntryCallback)
{
    int consumed = 0;
    int kid = GetKeyID(eventKey);
    int realEventType = eventType;
    BindingMap *mapping = KeyboardMappingForWindow(window);

    if(!mapping)
        return;

    if(eventKey != Key_Unmapped){
        int currentKeyState = keyStates[kid];
        if(eventType == KEYBOARD_EVENT_PRESS){
            if(currentKeyState != KEYBOARD_EVENT_RELEASE){
                realEventType = KEYBOARD_EVENT_REPEAT; // TODO: Check if this is enough
            }
        }

        keyStates[kid] = realEventType;
    }else if(rawKeyCode != 0){
        // I'm somehow getting a 0 code for UTF-8 that only happens once
        // i'm not finding any references to this so I'm skip it

        unmappedKeyState = eventType;
    }

    if(realEventType != KEYBOARD_EVENT_RELEASE && eventKey != Key_Unmapped){
        // Check bindings, only if no unmapped key is active
        if(mapping && unmappedKeyState == KEYBOARD_EVENT_RELEASE){
            consumed = KeyboardProcessBindingEvents(kid, mapping);
        }
    }

    if(mapping && utf8Size > 0 && eventType != KEYBOARD_EVENT_RELEASE
       && consumed == 0 && allowEntryCallback)
    {
        if(mapping->entryCallback){
            mapping->entryCallback(utf8Data, utf8Size, mapping->userPriv);
            consumed = 1;
        }
    }

    if(mapping && consumed == 0 && allowEntryCallback){
        if(mapping->entryRawCallback){
            mapping->entryRawCallback(rawKeyCode);
        }
    }

    for(auto it : keyboardActiveCbMapping){
        it.second();
    }
}

void RegisterKeyboardEventv(BindingMap *mapping, int repeat, const char *name,
                            KeySetEventCallback *callback, va_list args)
{
    int size = 0;
    int iKey = 0;
    Binding *binds = mapping->bindings;
    int *counts = mapping->bindingCount;
    Key keySet[KEYSET_MAX_SIZE];
    do{
        iKey = va_arg(args, int);
        if(iKey == -1) break;
        AssertA(iKey >= (int)Key_A && iKey < (int)Key_Unmapped, "Invalid key");
        keySet[size++] = (Key)iKey;
    }while(size < KEYSET_MAX_SIZE);

    AssertA(size > 0, "Invalid mapping");

    for(int i = 0; i < size; i++){
        int id = GetKeyID(keySet[i]);
        int count = counts[id];
        AssertA(count < MAX_BINDINGS_PER_ENTRY, "Requested too many binding");

        Binding *binding = &binds[GetBindingID(id, count)];
        if(name){
            memcpy(binding->bindingname, name, strlen(name));
        }
        memcpy(binding->keySet, keySet, sizeof(keySet));
        binding->keySetSize = size;
        binding->callback = callback;
        binding->supportsRepeat = repeat;
        counts[id] = count+1;
    }
}

void RegisterKeyboardEventEx(BindingMap *mapping, int repeat, const char *name,
                             KeySetEventCallback *callback, ...)
{
    va_list args;
    va_start(args, callback);
    RegisterKeyboardEventv(mapping, repeat, name, callback, args);
    va_end(args);
}

void RegisterKeyboardDefaultEntry(BindingMap *mapping, KeyEntryCallback *callback,
                                  void *userData)
{
    mapping->entryCallback = callback;
    mapping->userPriv = userData;
}

void RegisterKeyboardRawEntry(BindingMap *mapping, KeyRawEntryCallback *callback){
    mapping->entryRawCallback = callback;
}

BindingMap *KeyboardCreateMapping(){
    int keyCodes = GetMappedKeysCount();
    BindingMap *mapping = (BindingMap *)AllocatorGet(sizeof(BindingMap));
    int allocationSize = MAX_BINDINGS_PER_ENTRY * keyCodes;

    mapping->bindings = (Binding *)AllocatorGet(sizeof(Binding) * allocationSize);
    mapping->bindingCount = (int *)AllocatorGet(sizeof(int) * keyCodes);
    mapping->entryCallback = nullptr;
    mapping->entryRawCallback = nullptr;
    mapping->refWindow = nullptr;

    for(int i = 0; i < allocationSize; i++){
        Binding *binding = &mapping->bindings[i];
        binding->keySetSize = 0;
        binding->supportsRepeat = 0;
        if(i < keyCodes){
            mapping->bindingCount[i] = 0;
        }
    }

    //TODO: Bind some default stuff here
    return mapping;
}

BindingMap *KeyboardCreateMapping(void *refWindow){
    BindingMap *mapping = KeyboardCreateMapping();
    KeyboardSetReferenceWindow(mapping, refWindow);
    return mapping;
}

void KeyboardInitMappings(){
    int keyCodes = CreateKeyTable();
    keyStates = (int *)AllocatorGet(sizeof(int) * keyCodes);
    for(int i = 0; i < keyCodes; i++){
        keyStates[i] = KEYBOARD_EVENT_RELEASE;
    }
}

void KeyboardResetState(){
    int keyCodes = CreateKeyTable();
    for(int i = 0; i < keyCodes; i++){
        keyStates[i] = KEYBOARD_EVENT_RELEASE;
    }
}

const char *KeyboardGetStateName(int state){
    switch(state){
        case KEYBOARD_EVENT_RELEASE: return "KEYBOARD_EVENT_RELEASE";
        case KEYBOARD_EVENT_PRESS: return "KEYBOARD_EVENT_PRESS";
        case KEYBOARD_EVENT_REPEAT: return "KEYBOARD_EVENT_REPEAT";
        default: return "KEYBOARD_EVENT_UNMAPPED";
    }
}

int IsKeyModifier(Key key){
    return key == Key_LeftShift ||
           key == Key_LeftControl ||
           key == Key_LeftAlt ||
           key == Key_RightShift ||
           key == Key_RightControl ||
           key == Key_RightAlt;
}

int IsKeySpecialBinding(Key key){
    return key == Key_Tab ||
           key == Key_Space ||
           key == Key_Backspace ||
           key == Key_Right ||
           key == Key_Left ||
           key == Key_Down ||
           key == Key_Up ||
           key == Key_Enter;
}

const char *KeyboardGetKeyName(Key key){
    switch(key){
        case Key_A: return "Key_A";
        case Key_B: return "Key_B";
        case Key_C: return "Key_C";
        case Key_D: return "Key_D";
        case Key_E: return "Key_E";
        case Key_F: return "Key_F";
        case Key_G: return "Key_G";
        case Key_H: return "Key_H";
        case Key_I: return "Key_I";
        case Key_J: return "Key_J";
        case Key_K: return "Key_K";
        case Key_L: return "Key_L";
        case Key_M: return "Key_M";
        case Key_N: return "Key_N";
        case Key_O: return "Key_O";
        case Key_P: return "Key_P";
        case Key_Q: return "Key_Q";
        case Key_R: return "Key_R";
        case Key_S: return "Key_S";
        case Key_T: return "Key_T";
        case Key_U: return "Key_U";
        case Key_V: return "Key_V";
        case Key_X: return "Key_X";
        case Key_W: return "Key_W";
        case Key_Y: return "Key_Y";
        case Key_Z: return "Key_Z";
        case Key_0: return "Key_0";
        case Key_1: return "Key_1";
        case Key_2: return "Key_2";
        case Key_3: return "Key_3";
        case Key_4: return "Key_4";
        case Key_5: return "Key_5";
        case Key_6: return "Key_6";
        case Key_7: return "Key_7";
        case Key_8: return "Key_8";
        case Key_F1: return "Key_F1";
        case Key_F2: return "Key_F2";
        case Key_F3: return "Key_F3";
        case Key_F4: return "Key_F4";
        case Key_F5: return "Key_F5";
        case Key_F6: return "Key_F6";
        case Key_F7: return "Key_F7";
        case Key_F8: return "Key_F8";
        case Key_F9: return "Key_F9";
        case Key_F10: return "Key_F10";
        case Key_F11: return "Key_F11";
        case Key_F12: return "Key_F12";
        case Key_Comma: return "Key_Comma";
        case Key_Period: return "Key_Period";
        case Key_Slash: return "Key_Slash";
        case Key_Semicolon: return "Key_Semicolon";
        case Key_Equal: return "Key_Equal";
        case Key_Minus: return "Key_Minus";
        case Key_LeftBracket: return "Key_LeftBracket";
        case Key_Backslash: return "Key_Backslash";
        case Key_RightBracket: return "Key_RightBracket";
        case Key_Escape: return "Key_Escape";
        case Key_Tab: return "Key_Tab";
        case Key_Space: return "Key_Space";
        case Key_Backspace: return "Key_Backspace";
        case Key_Insert: return "Key_Insert";
        case Key_Delete: return "Key_Delete";
        case Key_Right: return "Key_Right";
        case Key_Left: return "Key_Left";
        case Key_Down: return "Key_Down";
        case Key_Up: return "Key_Up";
        case Key_PageUp: return "Key_PageUp";
        case Key_PageDown: return "Key_PageDown";
        case Key_Home: return "Key_Home";
        case Key_End: return "Key_End";
        case Key_LeftShift: return "Key_LeftShift";
        case Key_LeftControl: return "Key_LeftControl";
        case Key_LeftAlt: return "Key_LeftAlt";
        case Key_RightShift: return "Key_RightShift";
        case Key_RightControl: return "Key_RightControl";
        case Key_RightAlt: return "Key_RightAlt";
        case Key_Enter: return "Key_Enter";
        default: return "Unmapped";
    }
}
