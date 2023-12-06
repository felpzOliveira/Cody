#include <keyboard.h>
#include <string.h>

static int keycodes[512];
static int scancodes[Key_Unmapped + 2];

// Must implement these
int TranslateKey(int scancode) {
    if (scancode < 0 || scancode > 511){
        return Key_Unmapped;
    }
    return keycodes[scancode];
}

int GetMappedKeysCount() {
    return 512;
}

int CreateKeyTable() {
    for (int scancode = 0; scancode < 512; scancode++)
        keycodes[scancode] = Key_Unmapped;

    for (int index = 0; index < (int)Key_Unmapped + 2; index++)
        scancodes[index] = -1;

    keycodes[0x00B] = Key_0;
    keycodes[0x002] = Key_1;
    keycodes[0x003] = Key_2;
    keycodes[0x004] = Key_3;
    keycodes[0x005] = Key_4;
    keycodes[0x006] = Key_5;
    keycodes[0x007] = Key_6;
    keycodes[0x008] = Key_7;
    keycodes[0x009] = Key_8;
    keycodes[0x00A] = Key_9;
    keycodes[0x01E] = Key_A;
    keycodes[0x030] = Key_B;
    keycodes[0x02E] = Key_C;
    keycodes[0x020] = Key_D;
    keycodes[0x012] = Key_E;
    keycodes[0x021] = Key_F;
    keycodes[0x022] = Key_G;
    keycodes[0x023] = Key_H;
    keycodes[0x017] = Key_I;
    keycodes[0x024] = Key_J;
    keycodes[0x025] = Key_K;
    keycodes[0x026] = Key_L;
    keycodes[0x032] = Key_M;
    keycodes[0x031] = Key_N;
    keycodes[0x018] = Key_O;
    keycodes[0x019] = Key_P;
    keycodes[0x010] = Key_Q;
    keycodes[0x013] = Key_R;
    keycodes[0x01F] = Key_S;
    keycodes[0x014] = Key_T;
    keycodes[0x016] = Key_U;
    keycodes[0x02F] = Key_V;
    keycodes[0x011] = Key_W;
    keycodes[0x02D] = Key_X;
    keycodes[0x015] = Key_Y;
    keycodes[0x02C] = Key_Z;
#if 0
    keycodes[0x028] = Key_APOSTROPHE;
    keycodes[0x029] = Key_GRAVE_ACCENT;
    keycodes[0x01A] = Key_LEFT_BRACKET;
    keycodes[0x056] = Key_WORLD_2;
    keycodes[0x15D] = Key_MENU;
    keycodes[0x045] = Key_PAUSE;
    keycodes[0x03A] = Key_CAPS_LOCK;
    keycodes[0x145] = Key_NUM_LOCK;
    keycodes[0x046] = Key_SCROLL_LOCK;
    keycodes[0x15B] = Key_LEFT_SUPER;
    keycodes[0x137] = Key_PRINT_SCREEN;
    keycodes[0x15C] = Key_RIGHT_SUPER;
    keycodes[0x04E] = Key_ADD;
    keycodes[0x135] = Key_KP_DIVIDE;
    keycodes[0x037] = Key_KP_MULTIPLY;
#endif
    //keycodes[0x02B] = Key_Backslash;
    keycodes[0x056] = Key_Backslash;
    keycodes[0x033] = Key_Comma;
    keycodes[0x00D] = Key_Equal;

    keycodes[0x00C] = Key_Minus;
    keycodes[0x034] = Key_Period;
    keycodes[0x01B] = Key_RightBracket;
    keycodes[0x027] = Key_Semicolon;
    keycodes[0x035] = Key_Semicolon;
    keycodes[0x073] = Key_Slash;

    keycodes[0x00E] = Key_Backspace;
    keycodes[0x153] = Key_Delete;
    keycodes[0x14F] = Key_End;
    keycodes[0x01C] = Key_Enter;
    keycodes[0x001] = Key_Escape;
    keycodes[0x147] = Key_Home;
    keycodes[0x152] = Key_Insert;

    keycodes[0x151] = Key_PageDown;
    keycodes[0x149] = Key_PageUp;
    keycodes[0x039] = Key_Space;
    keycodes[0x00F] = Key_Tab;

    keycodes[0x03B] = Key_F1;
    keycodes[0x03C] = Key_F2;
    keycodes[0x03D] = Key_F3;
    keycodes[0x03E] = Key_F4;
    keycodes[0x03F] = Key_F5;
    keycodes[0x040] = Key_F6;
    keycodes[0x041] = Key_F7;
    keycodes[0x042] = Key_F8;
    keycodes[0x043] = Key_F9;
    keycodes[0x044] = Key_F10;
    keycodes[0x057] = Key_F11;
    keycodes[0x058] = Key_F12;
#if 0
    keycodes[0x064] = Key_F13;
    keycodes[0x065] = Key_F14;
    keycodes[0x066] = Key_F15;
    keycodes[0x067] = Key_F16;
    keycodes[0x068] = Key_F17;
    keycodes[0x069] = Key_F18;
    keycodes[0x06A] = Key_F19;
    keycodes[0x06B] = Key_F20;
    keycodes[0x06C] = Key_F21;
    keycodes[0x06D] = Key_F22;
    keycodes[0x06E] = Key_F23;
    keycodes[0x076] = Key_F24;
#endif
    keycodes[0x038] = Key_LeftAlt;
    keycodes[0x01D] = Key_LeftControl;
    keycodes[0x02A] = Key_LeftShift;


    keycodes[0x138] = Key_RightAlt;
    keycodes[0x11D] = Key_RightControl;
    keycodes[0x036] = Key_RightShift;

    keycodes[0x150] = Key_Down;
    keycodes[0x14B] = Key_Left;
    keycodes[0x14D] = Key_Right;
    keycodes[0x148] = Key_Up;

    keycodes[0x052] = Key_0;
    keycodes[0x04F] = Key_1;
    keycodes[0x050] = Key_2;
    keycodes[0x051] = Key_3;
    keycodes[0x04B] = Key_4;
    keycodes[0x04C] = Key_5;
    keycodes[0x04D] = Key_6;
    keycodes[0x047] = Key_7;
    keycodes[0x048] = Key_8;
    keycodes[0x049] = Key_9;

    keycodes[0x053] = Key_Period;
    keycodes[0x11C] = Key_Enter;
    keycodes[0x059] = Key_Equal;

    keycodes[0x04A] = Key_Minus;

    for (int scancode = 0; scancode < 512; scancode++){
        if (keycodes[scancode] > 0)
            scancodes[keycodes[scancode]] = scancode;
    }

    return 512;
}

