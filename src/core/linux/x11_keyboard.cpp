#include <x11_keyboard.h>
#include <types.h>
#include <string.h>
#include <stdlib.h>

static int keycodes[256];

extern LibHelperX11 x11Helper;

int TranslateKey(int scancode){
    if(scancode < 0 || scancode > 255){
        return Key_Unmapped;
    }
    return keycodes[scancode];
}

int TranslateKeyCode(int scancode){
    int keySym = Key_Unmapped;
    if(scancode < 8 || scancode > 255) return keySym;

    if(x11Helper.xkb.available){
        keySym = XkbKeycodeToKeysym(x11Helper.display, scancode, x11Helper.xkb.group, 1);
        switch(keySym){
            case XK_KP_0:           return Key_0;
            case XK_KP_1:           return Key_1;
            case XK_KP_2:           return Key_2;
            case XK_KP_3:           return Key_3;
            case XK_KP_4:           return Key_4;
            case XK_KP_5:           return Key_5;
            case XK_KP_6:           return Key_6;
            case XK_KP_7:           return Key_7;
            case XK_KP_8:           return Key_8;
            case XK_KP_9:           return Key_9;
            case XK_KP_Equal:       return Key_Equal;
            case XK_KP_Enter:       return Key_Enter;
            default:                break;
        }
        keySym = XkbKeycodeToKeysym(x11Helper.display, scancode, x11Helper.xkb.group, 0);
    }else{
        int dummy;
        KeySym* keySyms;
        keySyms = XGetKeyboardMapping(x11Helper.display, scancode, 1, &dummy);
        keySym = keySyms[0];
        XFree(keySyms);
    }

    switch(keySym){
        case XK_Escape:         return Key_Escape;
        case XK_Tab:            return Key_Tab;
        case XK_Shift_L:        return Key_LeftShift;
        case XK_Shift_R:        return Key_RightShift;
        case XK_Control_L:      return Key_LeftControl;
        case XK_Control_R:      return Key_RightControl;
        case XK_Meta_L:
        case XK_Alt_L:          return Key_LeftAlt;
        case XK_Mode_switch: // Mapped to Alt_R on many keyboards
        case XK_ISO_Level3_Shift: // AltGr on at least some machines
        case XK_Meta_R:
        case XK_Alt_R:          return Key_RightAlt;
        case XK_Delete:         return Key_Delete;
        case XK_BackSpace:      return Key_Backspace;
        case XK_Return:         return Key_Enter;
        case XK_Home:           return Key_Home;
        case XK_End:            return Key_End;
        case XK_Page_Up:        return Key_PageUp;
        case XK_Page_Down:      return Key_PageDown;
        case XK_Insert:         return Key_Insert;
        case XK_Left:           return Key_Left;
        case XK_Right:          return Key_Right;
        case XK_Down:           return Key_Down;
        case XK_Up:             return Key_Up;
        case XK_F1:             return Key_F1;
        case XK_F2:             return Key_F2;
        case XK_F3:             return Key_F3;
        case XK_F4:             return Key_F4;
        case XK_F5:             return Key_F5;
        case XK_F6:             return Key_F6;
        case XK_F7:             return Key_F7;
        case XK_F8:             return Key_F8;
        case XK_F9:             return Key_F9;
        case XK_F10:            return Key_F10;
        case XK_F11:            return Key_F11;
        case XK_F12:            return Key_F12;

        case XK_KP_Insert:      return Key_0;
        case XK_KP_End:         return Key_1;
        case XK_KP_Down:        return Key_2;
        case XK_KP_Page_Down:   return Key_3;
        case XK_KP_Left:        return Key_4;
        case XK_KP_Right:       return Key_6;
        case XK_KP_Home:        return Key_7;
        case XK_KP_Up:          return Key_8;
        case XK_KP_Page_Up:     return Key_9;
        case XK_KP_Delete:      return Key_Delete;
        case XK_KP_Equal:       return Key_Equal;
        case XK_KP_Enter:       return Key_Enter;

        case XK_a:              return Key_A;
        case XK_b:              return Key_B;
        case XK_c:              return Key_C;
        case XK_d:              return Key_D;
        case XK_e:              return Key_E;
        case XK_f:              return Key_F;
        case XK_g:              return Key_G;
        case XK_h:              return Key_H;
        case XK_i:              return Key_I;
        case XK_j:              return Key_J;
        case XK_k:              return Key_K;
        case XK_l:              return Key_L;
        case XK_m:              return Key_M;
        case XK_n:              return Key_N;
        case XK_o:              return Key_O;
        case XK_p:              return Key_P;
        case XK_q:              return Key_Q;
        case XK_r:              return Key_R;
        case XK_s:              return Key_S;
        case XK_t:              return Key_T;
        case XK_u:              return Key_U;
        case XK_v:              return Key_V;
        case XK_w:              return Key_W;
        case XK_x:              return Key_X;
        case XK_y:              return Key_Y;
        case XK_z:              return Key_Z;
        case XK_1:              return Key_1;
        case XK_2:              return Key_2;
        case XK_3:              return Key_3;
        case XK_4:              return Key_4;
        case XK_5:              return Key_5;
        case XK_6:              return Key_6;
        case XK_7:              return Key_7;
        case XK_8:              return Key_8;
        case XK_9:              return Key_9;
        case XK_0:              return Key_0;
        case XK_space:          return Key_Space;
        case XK_minus:          return Key_Minus;
        case XK_plus:           return Key_Plus;
        case XK_equal:          return Key_Equal;
        case XK_bracketleft:    return Key_LeftBracket;
        case XK_bracketright:   return Key_RightBracket;
        case XK_backslash:      return Key_Backslash;
        case XK_semicolon:      return Key_Semicolon;
        case XK_comma:          return Key_Comma;
        case XK_period:         return Key_Period;
        case XK_slash:          return Key_Slash;
        default:                break;
    }

    return Key_Unmapped;
}

int GetMappedKeysCount(){
    return 256;
}

int CreateKeyTable(){
    for(int scancode = 0; scancode < 256; scancode++){
        keycodes[scancode] = TranslateKeyCode(scancode);
    }

    return 256;
}
