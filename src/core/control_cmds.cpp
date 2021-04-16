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
}ControlCmds;

static ControlCmds controlCmds;

void ControlCmdsTrigger(){
    controlCmds.lastMapping = KeyboardGetActiveMapping();
    KeyboardSetActiveMapping(controlCmds.mapping);
}

void ControlCommands_YieldKeyboard(){
    if(controlCmds.lastMapping){
        KeyboardSetActiveMapping(controlCmds.lastMapping);
    }
}

// Capture keys that were not bounded and generated events
// these events simply yield the keyboard
void ControlCmdsDefaultEntry(char *utf8Data, int utf8Size){
    (void)utf8Data; (void)utf8Size;
    // This is generic, but should it be binded?
    if(StringIsDigits(utf8Data, utf8Size)){
        bool swapped = false;
        uint index = StringToUnsigned(utf8Data, utf8Size);
        // Select this view
        ViewTreeIterator iterator;
        ViewTree_Begin(&iterator);
        int id = -1;

        std::stack<View *> viewStack;
        if(iterator.value) id = 1;
        while(iterator.value){
            if(iterator.value->view){
                viewStack.push(iterator.value->view);
            }

            if(iterator.value->view && (uint)id == index && !swapped){
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
        ControlCommands_YieldKeyboard();
    }
}

void ControlCmdsRenderIndices(){
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);
    while(iterator.value){
        if(iterator.value->view){
            View_SetControlOpts(iterator.value->view, Control_Opts_Indices,
                                kTransitionControlIndices);
        }

        ViewTree_Next(&iterator);
    }

    Timing_Update();
}

void ControlCommands_Initialize(){
    BindingMap *mapping = KeyboardCreateMapping();

    RegisterKeyboardDefaultEntry(mapping, ControlCmdsDefaultEntry);
    RegisterRepeatableEvent(mapping, ControlCommands_YieldKeyboard, Key_Escape);
    RegisterRepeatableEvent(mapping, ControlCmdsRenderIndices, Key_Q);

    controlCmds.mapping = mapping;
    controlCmds.lastMapping = nullptr;
}

void ControlCommands_BindTrigger(BindingMap *mapping){
    RegisterRepeatableEvent(mapping, ControlCmdsTrigger, Key_LeftControl, Key_B);
}
