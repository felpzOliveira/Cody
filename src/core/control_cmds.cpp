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
        if(ControlCommands_IsExpanded()) return;

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
        }
    }else{
        ControlCommands_YieldKeyboard();
    }

    FinishEvent();
}

void ControlCmdsClear(){
    ControlCommands_YieldKeyboard();
    FinishEvent();
}

void ControlCmdsRestoreCurrent(){
    if(controlCmds.is_expanded){
        ViewTree_ExpandRestore();
        FinishEvent();
    }
}

int ControlCommands_IsExpanded(){
    return controlCmds.is_expanded;
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
    AppSetRenderViewIndices(0);
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
    AppSetRenderViewIndices(1);
    StartEvent();
}

void ControlCommands_RestoreExpand(){
    if(controlCmds.is_expanded){
        ViewTree_ExpandRestore();
        controlCmds.is_expanded = 0;
    }
    FinishEvent();
}

// Up/right => direction = 1, 0 otherwise
void ControlCommands_ViewArrowSelect(int direction, int axis){
    NullRet(!ControlCommands_IsExpanded());
    int other_axis = (axis+1)%2;
    View *view = AppGetActiveView();
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);

    ViewNode *vnode = iterator.value;
    bool found = false;
    // find the current view and get the baseX value
    while(iterator.value){
        if(view == iterator.value->view){
            vnode = iterator.value;
            found = true;
            break;
        }
        ViewTree_Next(&iterator);
    }

    if(found){
        auto cmp = [&](ViewNode *nodeA, ViewNode *nodeB) -> int{
            if(direction == 1)
                return nodeA->geometry.lower[axis] < nodeB->geometry.lower[axis];
            else
                return nodeA->geometry.lower[axis] > nodeB->geometry.lower[axis];
        };
        ViewNode *bestfit = nullptr;
        PriorityQueue<ViewNode> *pq = PriorityQueue_Create<ViewNode>(cmp);
        vec2ui lower = vnode->geometry.lower;
        vec2ui upper = vnode->geometry.upper;
        uint dif = 99999;
        ViewTree_Begin(&iterator);

        while(iterator.value){
            if(iterator.value != vnode){
                bool insert = false;
                if(direction == 1)
                    insert = iterator.value->geometry.upper[axis] > upper[axis];
                else
                    insert = iterator.value->geometry.lower[axis] < lower[axis];
                if(insert){
                    PriorityQueue_Push(pq, iterator.value);
                }
            }
            ViewTree_Next(&iterator);
        }

        PriorityQueue_ForAllItems(pq, [&](ViewNode *pqNode) -> int{
            uint cur_dif = dif;
            if(direction == 1 && axis == 1)
                cur_dif = Absf((Float)pqNode->geometry.lower[other_axis] -
                                (Float)lower[other_axis]);
            else if(direction == 1 && axis == 0)
                cur_dif = Absf((Float)pqNode->geometry.upper[other_axis] -
                                (Float)upper[other_axis]);
            else if(direction == 1)
                cur_dif = Absf((Float)pqNode->geometry.lower[other_axis] -
                                (Float)lower[other_axis]);
            else
                cur_dif = Absf((Float)pqNode->geometry.upper[other_axis] -
                                (Float)upper[other_axis]);

            if(cur_dif < dif){
                bestfit = pqNode;
                dif = cur_dif;
            }
            return 1;
        });

        if(bestfit){
            if(BufferView_IsVisible(&bestfit->view->bufferView)){
                ViewTree_SetActive(bestfit);
                AppSetActiveView(bestfit->view);
            }
        }
        PriorityQueue_Free(pq);
    }else{
        BUG();
        BUG_PRINT("Could not locate the active view in view tree");
    }
    ControlCmdsClear();
}

void ControlCommands_ViewDown(){
    ControlCommands_ViewArrowSelect(0, 1);
}

void ControlCommands_ViewUp(){
    ControlCommands_ViewArrowSelect(1, 1);
}

void ControlCommands_ViewRight(){
    ControlCommands_ViewArrowSelect(1, 0);
}

void ControlCommands_ViewLeft(){
    ControlCommands_ViewArrowSelect(0, 0);
}

void ControlCommands_Initialize(){
    BindingMap *mapping = KeyboardCreateMapping();

    RegisterKeyboardDefaultEntry(mapping, ControlCmdsDefaultEntry, nullptr);

    RegisterRepeatableEvent(mapping, ControlCommands_YieldKeyboard, Key_Escape);
    RegisterRepeatableEvent(mapping, ControlCmdsRenderIndices, Key_Q);
    RegisterRepeatableEvent(mapping, ControlCmdsExpandCurrent, Key_Z);
    RegisterRepeatableEvent(mapping, ControlCmdsClear, Key_Space);
    RegisterRepeatableEvent(mapping, ControlCommands_ViewLeft, Key_Left);
    RegisterRepeatableEvent(mapping, ControlCommands_ViewRight, Key_Right);
    RegisterRepeatableEvent(mapping, ControlCommands_ViewDown, Key_Down);
    RegisterRepeatableEvent(mapping, ControlCommands_ViewUp, Key_Up);

    controlCmds.mapping = mapping;
    controlCmds.lastMapping = nullptr;
    controlCmds.is_expanded = 0;
}

void ControlCommands_BindTrigger(BindingMap *mapping){
    RegisterRepeatableEvent(mapping, ControlCmdsTrigger, Key_LeftControl, Key_B);
}
