#include <view.h>

const char *View_GetStateString(ViewState state){
#define STR_CASE(x) case x : return #x
    switch(state){
        STR_CASE(View_FreeTyping);
        STR_CASE(View_QueryBar);
        STR_CASE(View_SelectableList);
        STR_CASE(View_AutoComplete);
        default: return "None";
    }
#undef STR_CASE
}

static void View_SelectableListSetLineBuffer(View *view, LineBuffer *sourceBuffer){
    SelectableList *list = &view->selectableList;
    SelectableList_SetLineBuffer(list, sourceBuffer);
}

//TODO: Adjust as needed
ViewState View_GetDefaultState(){
    return View_FreeTyping;
}

vec2ui View_SelectableListGetViewRange(View *view){
    SelectableList *list = &view->selectableList;
    return SelectableList_GetViewRange(list);
}

int View_SelectableListGetActiveIndex(View *view){
    return SelectableList_GetActiveIndex(&view->selectableList);
}

uint View_SelectableListGetRealIndex(View *view, uint i){
    SelectableList *list = &view->selectableList;
    return SelectableList_GetRealIndex(list, i);
}

FileOpener *View_GetFileOpener(View *view){
    return view->fileOpener;
}

LineBuffer *View_SelectableListGetLineBuffer(View *view){
    return view->selectableList.listBuffer;
}

LineBuffer *View_GetBufferViewLineBuffer(View *view){
    return view->bufferView.lineBuffer;
}

BufferView *View_GetBufferView(View *view){
    return &view->bufferView;
}

void View_GetControlRenderOpts(View *view, ControlProps **props){
    *props = &view->controlProps;
}

void View_SetControlOpts(View *view, ControlRenderOpts opts, Float inter){
    view->controlProps.opts = opts;
}

QueryBar *View_GetQueryBar(View *view){
    return &view->queryBar;
}

RenderList *View_GetRenderPipeline(View *view){
    return &view->renderList[view->state];
}

void View_Free(View *view){
    if(view->selectableList.selectableSize > 0){
        AllocatorFree(view->selectableList.selectable);
    }

    QueryBar_Free(&view->queryBar);

    view->selectableList = SELECTABLE_LIST_INITIALIZER;
}

void View_SelectableListGetItem(View *view, uint i, Buffer **buffer){
    SelectableList *list = &view->selectableList;
    SelectableList_GetItem(list, i, buffer);
}

void View_SelectableListSwapList(View *view, LineBuffer *sourceBuffer, int release){
    SelectableList *list = &view->selectableList;
    SelectableList_SwapList(list, sourceBuffer, release);
}

void View_SelectableListFilterBy(View *view, char *key, uint keylen){
    SelectableList *list = &view->selectableList;
    SelectableList_Filter(list, key, keylen);
}

void View_SelectableListPush(View *view, char *item, uint len){
    SelectableList_Push(&view->selectableList, item, len);
}

void View_SelectableListSet(View *view, LineBuffer *sourceBuffer,
                            char *title, uint titlelen, OnQueryBarEntry entry,
                            OnQueryBarCancel cancel, OnQueryBarCommit commit,
                            QueryBarInputFilter *filter)
{
    AssertA(sourceBuffer != nullptr, "Invalid source buffer for view list");
    SelectableList *list = &view->selectableList;
    list->viewRange = vec2ui(0, list->currentLineRange);
    View_SelectableListSetLineBuffer(view, sourceBuffer);
    View_ReturnToState(view, View_SelectableList);
    QueryBar_ActivateCustom(&view->queryBar, title, titlelen, entry,
                            cancel, commit, filter);
}

Transform View_GetTranslateTransform(View *view){
    return view->transforms.translate;
}

void View_SetTranslateTransform(View *view, Transform translate){
    view->transforms.translate = translate;
}

void View_ResetTransforms(View *view){
    view->transforms.translate = Transform();
}

void View_EarlyInitialize(View *view){
    view->descLocation = DescriptionTop; // TODO: Config
    view->fileOpener = AllocatorGetN(FileOpener, 1);
    view->autoCompleteList = AllocatorGetN(SelectableList, 1);

    View_ResetTransforms(view);

    SelectableList_Init(&view->selectableList);
    SelectableList_Init(view->autoCompleteList);
    QueryBar_Initialize(&view->queryBar);

    View_SetControlOpts(view, Control_Opts_None);

    view->renderList[View_FreeTyping] = {
        .stages = {
            Graphics_RenderView,
            Graphics_RenderControlCommands,
        },
        .stageCount = 2,
    };

    view->renderList[View_QueryBar] = {
        .stages = {
            Graphics_RenderView,
            Graphics_RenderQueryBar,
            Graphics_RenderControlCommands,
        },
        .stageCount = 3,
    };

    view->renderList[View_SelectableList] = {
        .stages = {
            Graphics_RenderListSelector,
            Graphics_RenderQueryBar,
            Graphics_RenderControlCommands,
        },
        .stageCount = 3,
    };

    view->renderList[View_AutoComplete] = {
        .stages = {
            Graphics_RenderView,
            Graphics_RenderAutoComplete,
            Graphics_RenderControlCommands,
        },

        .stageCount = 3,
    };
}

void View_SetGeometry(View *view, Geometry geometry, uint lineHeight){
    Geometry barGeo;
    Float height = geometry.upper.y - geometry.lower.y;
    Float acHeight = kAutoCompleteMaxHeightFraction * height;
    Float realHeight = kViewSelectableListScaling * lineHeight;
    Float pHeight = (1.0 + kViewSelectableListOffset) * realHeight;
    Float listLines = height / pHeight;
    uint count = (uint)Floor(listLines);
    uint displayable = (uint)Floor(height / realHeight) - 1;

    view->geometry = geometry;

    SelectableList_SetView(&view->selectableList, count, displayable);

    if(view->descLocation == DescriptionTop){
        barGeo.lower = vec2ui(geometry.lower.x, geometry.upper.y - lineHeight);
        barGeo.upper = vec2ui(geometry.upper.x, geometry.upper.y);
    }else if(view->descLocation == DescriptionBottom){
        barGeo.lower = vec2ui(geometry.lower.x, geometry.lower.y);
        barGeo.upper = vec2ui(geometry.upper.x, geometry.lower.y + lineHeight);
    }else{
        AssertErr(0, "Unknown description");
    }

    barGeo.extensionX = geometry.extensionX;
    barGeo.extensionY = geometry.extensionY;
    BufferView_SetGeometry(&view->bufferView, geometry, lineHeight);
    QueryBar_SetGeometry(&view->queryBar, &barGeo, lineHeight);
    SelectableList_SetRange(&view->selectableList, vec2ui(0, count));

    realHeight = kAutoCompleteListScaling * lineHeight;
    pHeight = (1.0 + kViewSelectableListOffset) * realHeight;
    listLines = acHeight / pHeight;
    count = (uint)Floor(listLines);
    displayable = (uint)Floor(acHeight / realHeight) - 1;

    SelectableList_SetView(view->autoCompleteList, count, displayable);
    SelectableList_SetRange(view->autoCompleteList, vec2ui(0, count));
}

void View_SetActive(View *view, int isActive){
    view->isActive = isActive;
    view->queryBar.isActive = isActive;
    view->bufferView.isActive = isActive;
}

//TODO: Add as needed
void View_ReturnToState(View *view, ViewState state){
    if(View_IsQueryBarActive(view)){
        QueryBar_Reset(&view->queryBar, view, 0);
        //BufferView_StartCursorTransition(bView, source.x, source.y, kTransitionJumpInterval);
    }

    if(state == View_AutoComplete){
        view->autoCompleteList->active = 0;

    }

    view->state = state;
}

int View_PreviousItem(View *view){
    if(view->state == View_QueryBar){
        return QueryBar_PreviousItem(&view->queryBar, view);
    }else if(view->state == View_SelectableList){
        SelectableList *list = &view->selectableList;
        QueryBarInputFilter *filter = &view->queryBar.filter;
        int minv = filter->allowFreeType ? -1 : 0;
        SelectableList_PreviousItem(list, minv);
    }

    return 0;
}

int View_NextItem(View *view){
    if(view->state == View_QueryBar){
        return QueryBar_NextItem(&view->queryBar, view);
    }else if(view->state == View_SelectableList){
        SelectableList *list = &view->selectableList;
        SelectableList_NextItem(list);
    }

    return 0;
}

int View_CommitToState(View *view, ViewState state){
    int c = 1;
    if(View_IsQueryBarActive(view)){
        c = QueryBar_Reset(&view->queryBar, view, 1);
    }

    if(c != 0)
        view->state = state;

    return c;
}

ViewState View_GetState(View *view){
    return view->state;
}

void View_SelectableListDebug(View *view){
    SelectableList *list = &view->selectableList;
    printf("List len = %u, Buffer len = %u, Active = %u\n",
           list->selectableSize, list->listBuffer->lineCount, list->active);

    for(uint i = 0; i < list->used; i++){
        Buffer *b = LineBuffer_GetBufferAt(list->listBuffer, list->selectable[i]);
        printf("State (%u) = %u (%s)\n", i, list->selectable[i], b->data);
    }
}
