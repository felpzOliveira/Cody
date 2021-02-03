#include <keyboard.h>
#include <x11_keyboard.h>
#include <types.h>
#include <stdarg.h>
#include <string.h>

#define GetKeyID(key) ((int)key - (int)Key_A)

static BindingMap *activeMapping = nullptr;
static int *keyStates;

const char *KeyboardGetKeyName(Key key);

void KeyboardSetActiveMapping(BindingMap *mapping){
    activeMapping = mapping;
}

inline int KeyboardGetKeyState(Key key){
    return keyStates[GetKeyID(key)];
}

void KeyboardProcessBindingState(Binding *binding){
    AssertA(binding != nullptr, "Invalid binding");
    AssertA(binding->callback != nullptr, "Invalid binding callback");
    for(int i = 0; i < binding->keySetSize; i++){
        if(KeyboardGetKeyState((Key)binding->keySet[i]) != KEYBOARD_EVENT_PRESS)
            return;
    }
    
    binding->callback();
}

void KeyboardEvent(Key eventKey, int eventType, char *utf8Data, int utf8Size){
    int kid = GetKeyID(eventKey);
    if(eventKey != Key_Unmapped){
        keyStates[kid] = eventType;
    }
    
    if(eventType == KEYBOARD_EVENT_PRESS){
        if(eventKey != Key_Unmapped){
            printf("Pressed: %s\n", KeyboardGetKeyName((Key)eventKey));
        }
        
        // Check bindings
        if(activeMapping){
            Binding *binds = activeMapping->bindings;
            int *counts = activeMapping->bindingCount;
            int count = counts[kid];
            for(int i = 0; i < count; i++){
                Binding *binding = &binds[kid * MAX_BINDINGS_PER_ENTRY + i];
                KeyboardProcessBindingState(binding);
            }
        }
    }else{
        if(eventKey != Key_Unmapped){
            printf("Released: %s\n", KeyboardGetKeyName((Key)eventKey));
        }
    }
}

void RegisterKeyboardEventv(BindingMap *mapping, KeySetEventCallback *callback,
                            va_list args)
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
        if(count < MAX_BINDINGS_PER_ENTRY){
            Binding *binding = &binds[id * MAX_BINDINGS_PER_ENTRY + count];
            memcpy(binding->keySet, keySet, sizeof(keySet));
            binding->keySetSize = size;
            binding->callback = callback;
            counts[id] = count+1;
        }else{
            AssertA(0, "Too many bindings for key");
        }
    }
}

void RegisterKeyboardEventEx(BindingMap *mapping, KeySetEventCallback *callback, ...){
    va_list args;
    va_start(args, callback);
    RegisterKeyboardEventv(mapping, callback, args);
    va_end(args);
}

BindingMap *KeyboardCreateMapping(){
    int keyCodes = GetMappedKeysCountX11();
    BindingMap *mapping = (BindingMap *)AllocatorGet(sizeof(BindingMap));
    int allocationSize = MAX_BINDINGS_PER_ENTRY * keyCodes;
    
    mapping->bindings = (Binding *)AllocatorGet(sizeof(Binding) * allocationSize);
    mapping->bindingCount = (int *)AllocatorGet(sizeof(int) * keyCodes);
    
    for(int i = 0; i < allocationSize; i++){
        Binding *binding = &mapping->bindings[i];
        binding->keySetSize = 0;
        if(i < keyCodes){
            mapping->bindingCount[i] = 0;
        }
    }
    
    //TODO: Bind some default stuff here
    return mapping;
}

void KeyboardInitMappings(){
    int keyCodes = CreateKeyTableX11();
    keyStates = (int *)AllocatorGet(sizeof(int) * keyCodes);
    memset(keyStates, KEYBOARD_EVENT_RELEASE, sizeof(int) * keyCodes);
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
        case Key_9: return "Key_9";
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
