#include <control_cmds.h>
#include <utilities.h>
#include <keyboard.h>
#include <app.h>
#include <timing.h>
#include <view_tree.h>
#include <stack>

typedef struct ControlCmds{
    BindingMap *mapping;
    BindingMap *lastMapping;
    int is_expanded;
}ControlCmds;

static ControlCmds controlCmds;

void ControlCmdsRestoreCurrent();
void FinishEvent();

void ControlCommands_YieldKeyboard(){
    if(controlCmds.lastMapping){
        KeyboardSetActiveMapping(controlCmds.lastMapping);
    }
}

void ControlCmdsTrigger(){
    controlCmds.lastMapping = KeyboardGetActiveMapping();
    KeyboardSetActiveMapping(controlCmds.mapping);
}

// Capture keys that were not bounded and generated events
// these events simply yield the keyboard
void ControlCmdsDefaultEntry(char *utf8Data, int utf8Size, void *){
    (void)utf8Data; (void)utf8Size;
    // This is generic, but should it be binded?

    // We are going to need to iterate the view tree in any case
    // so initialize a iterator
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);

    if(StringIsDigits(utf8Data, utf8Size)){
        bool swapped = false;
        uint index = StringToUnsigned(utf8Data, utf8Size);
        int id = -1;
        std::stack<View *> viewStack;

        if(iterator.value) id = 1;
        while(iterator.value){
            if(iterator.value->view){
                viewStack.push(iterator.value->view);
            }

            if(iterator.value->view && (uint)id == index && !swapped){
                ControlCmdsRestoreCurrent();
                AppSetActiveView(iterator.value->view);
                swapped = true;
            }

            id++;
            ViewTree_Next(&iterator);
        }

        if(!swapped){
            ControlCommands_YieldKeyboard();
        }else{
            // if we jump to a view than we need make sure we restore all views
            while(viewStack.size() > 0){
                View *view = viewStack.top();
                viewStack.pop();
                View_SetControlOpts(view, Control_Opts_None);
            }
        }
    }else{
        // clear the interface in case something was rendering from our side
        while(iterator.value){
            // I think we can default to always calling this and not get into
            // any trouble
            View_SetControlOpts(iterator.value->view, Control_Opts_None);
            ViewTree_Next(&iterator);
        }
        ControlCommands_YieldKeyboard();
    }

    FinishEvent();
}

void ControlCmdsClear(){
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View_SetControlOpts(iterator.value->view, Control_Opts_None);
        ViewTree_Next(&iterator);
    }
    ControlCommands_YieldKeyboard();
    FinishEvent();
}

void ControlCmdsRestoreCurrent(){
    if(controlCmds.is_expanded){
        ViewTree_ExpandRestore();
        FinishEvent();
    }
}

static void ControlCmdsHandleExpandRestore(int yield=1){
    uint count = ViewTree_GetCount();
    if(count <= 1) return;

    if(controlCmds.is_expanded){
        ViewTree_ExpandRestore();
    }else{
        ViewTree_ExpandCurrent();
    }

    if(yield){
        ControlCommands_YieldKeyboard();
    }
    controlCmds.is_expanded = 1 - controlCmds.is_expanded;
}

void ControlCmdsExpandCurrent(){
    ControlCmdsHandleExpandRestore(1);
    FinishEvent();
}

void ControlCommands_SwapExpand(){
    ControlCmdsHandleExpandRestore(0);
    FinishEvent();
}

static double startTime;
static bool finished = true;
bool IndicesEvent(){
    double currTime = GetElapsedTime();
    //printf("Running %g\n", currTime - startTime);
    if(currTime - startTime > 2 || finished){
        if(!finished){
            ControlCmdsClear();
        }
        return false;
    }
    return true;
}

void FinishEvent(){
    finished = true;
}

void StartEvent(){
    Float freq = 1.0 / 30.0;
    finished = false;
    startTime = GetElapsedTime();
    Graphics_AddEventHandler(freq, IndicesEvent);
    Timing_Update();
}

void ControlCmdsRenderIndices(){
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);
    while(iterator.value){
        if(iterator.value->view){
            if(BufferView_IsVisible(View_GetBufferView(iterator.value->view))){
                View_SetControlOpts(iterator.value->view, Control_Opts_Indices,
                                    kTransitionControlIndices);
            }
        }

        ViewTree_Next(&iterator);
    }

    StartEvent();
}

void ControlCommands_RestoreExpand(){
    if(controlCmds.is_expanded){
        ViewTree_ExpandRestore();
        controlCmds.is_expanded = 0;
    }
    FinishEvent();
}

void ControlCommands_Initialize(){
    BindingMap *mapping = KeyboardCreateMapping();

    RegisterKeyboardDefaultEntry(mapping, ControlCmdsDefaultEntry, nullptr);

    RegisterRepeatableEvent(mapping, ControlCommands_YieldKeyboard, Key_Escape);
    RegisterRepeatableEvent(mapping, ControlCmdsRenderIndices, Key_Q);
    RegisterRepeatableEvent(mapping, ControlCmdsExpandCurrent, Key_Z);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Space);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Left);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Right);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Down);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Up);

    controlCmds.mapping = mapping;
    controlCmds.lastMapping = nullptr;
    controlCmds.is_expanded = 0;
}

void ControlCommands_BindTrigger(BindingMap *mapping){
    RegisterRepeatableEvent(mapping, ControlCmdsTrigger, Key_LeftControl, Key_B);
}
