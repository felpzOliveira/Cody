#include <app.h>
#include <keyboard.h>
#include <lex.h>
#include <timing.h>

// grab some utilities
void AppRestoreAllViews();
void AppSetMouseHook(std::function<void(void)> hook);
void AppRemoveMouseHook();

struct DualModeControl{
    BindingMap *mapping;
    BindingMap *lastMapping;
    int currentState;
};

static DualModeControl modalControl;

int DualModeGetState(){
    return modalControl.currentState;
}

void DualModeDefaultEntry(char *, int, void *){
    //printf("DualMode capture\n");
}

void DualModeSetKeyboard(){
    KeyboardSetActiveMapping(modalControl.mapping);
}

void DualModeExit(){
    if(modalControl.lastMapping)
        KeyboardSetActiveMapping(modalControl.lastMapping);
    modalControl.currentState = ENTRY_MODE_FREE;
    AppRemoveMouseHook();
}

void DualModeEnter(){
    return;
    AppRestoreAllViews();
    AppSetMouseHook(DualModeSetKeyboard);
    modalControl.lastMapping = KeyboardGetActiveMapping();
    KeyboardSetActiveMapping(modalControl.mapping);
    modalControl.currentState = ENTRY_MODE_LOCK;
}

void DualModeOpenFile(){
    AppCommandOpenFileWithViewType(CodeView, OPEN_FILE_FLAGS_ALLOW_CREATION);
    AppSetDelayedCall(DualModeSetKeyboard);
}

void DualModeSwitchBuffer(){
    AppCommandSwitchBuffer();
    AppSetDelayedCall(DualModeSetKeyboard);
}

void DualModeSplitHorizontal(){
    AppSetDelayedCall(DualModeSetKeyboard);
    AppCommandSplitHorizontal();
}

void DualModeSplitVertical(){
    AppSetDelayedCall(DualModeSetKeyboard);
    AppCommandSplitVertical();
}

void DualModeSwapView(){
    AppSetDelayedCall(DualModeSetKeyboard);
    AppCommandSwapView();
}

void DualModeInit(){
    BindingMap *mapping = KeyboardCreateMapping();
    RegisterKeyboardDefaultEntry(mapping, DualModeDefaultEntry, nullptr);

    RegisterRepeatableEvent(mapping, DualModeExit, Key_A);

    // grab basic movement
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppCommandRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandUpArrow, Key_Up);
    RegisterRepeatableEvent(mapping, AppCommandDownArrow, Key_Down);
    RegisterRepeatableEvent(mapping, AppCommandSetGhostCursor, Key_Space,
                            Key_LeftControl);
    RegisterRepeatableEvent(mapping, DualModeSwapView, Key_LeftAlt, Key_W);

    // some file/buffer management
    RegisterRepeatableEvent(mapping, DualModeOpenFile, Key_LeftAlt, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_C);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_Q);
    RegisterRepeatableEvent(mapping, DualModeSwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, DualModeSplitHorizontal, Key_LeftAlt, Key_LeftControl, Key_H);
    RegisterRepeatableEvent(mapping, DualModeSplitVertical, Key_LeftAlt, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandSwapLineNbs, Key_LeftAlt, Key_N);

    modalControl.mapping = mapping;
    modalControl.currentState = ENTRY_MODE_FREE;
}

void DualMode_BindTrigger(BindingMap *mapping){
    RegisterRepeatableEvent(mapping, DualModeEnter, Key_LeftControl, Key_X);
}
