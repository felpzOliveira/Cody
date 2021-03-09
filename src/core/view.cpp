#include <view.h>

static void View_SelectableListUpdateRange(View *view){
    SelectableList *list = &view->selectableList;
    uint mIndex = list->viewRange.x + list->currentLineRange;
    if(list->active >= (int)mIndex){
        uint ry = list->active - mIndex + 1;
        list->viewRange.x += ry;
        list->viewRange.y += ry;
    }else if(list->active < (int)list->viewRange.x){
        uint range = list->viewRange.y - list->viewRange.x;
        list->viewRange.x = list->active;
        list->viewRange.y = list->active + range;
    }
}

static void View_SelectableListSetLineBuffer(View *view, LineBuffer *sourceBuffer){
    SelectableList *list = &view->selectableList;
    if(list->selectableSize == 0 || list->selectableSize < sourceBuffer->lineCount){
        if(list->selectableSize == 0){
            uint n = Max(sourceBuffer->lineCount+1, DefaultAllocatorSize);
            list->selectable = AllocatorGetN(uint, n);
            list->selectableSize = n;
        }else{
            uint n = sourceBuffer->lineCount + 1;
            list->selectable = AllocatorExpand(uint, list->selectable,
                                               n, sourceBuffer->lineCount);
            list->selectableSize = n;
        }
    }
    
    for(uint i = 0; i < list->selectableSize; i++){
        if(i < sourceBuffer->lineCount)
            list->selectable[i] = i;
        else
            list->selectable[i] = -1;
    }
    
    list->used = sourceBuffer->lineCount;
    list->viewRange.y = Min(list->currentDisplayRange, list->used);
    list->active = 0;
    view->selectableList.listBuffer = sourceBuffer;
}

//TODO: Adjust as needed
ViewState View_GetDefaultState(){
    return View_FreeTyping;
}

vec2ui View_SelectableListGetViewRange(View *view){
    SelectableList *list = &view->selectableList;
    return vec2ui(list->viewRange.x, Min(list->viewRange.y, list->listBuffer->lineCount));
}

int View_SelectableListGetActiveIndex(View *view){
    return view->selectableList.active;
}

uint View_SelectableListGetRealIndex(View *view, uint i){
    SelectableList *list = &view->selectableList;
    uint r = 0;
    if(i < list->used){
        r = list->selectable[i];
    }
    
    return r;
}

FileOpener *View_GetFileOpener(View *view){
    return view->fileOpener;
}

LineBuffer *View_SelectableListGetLineBuffer(View *view){
    return view->selectableList.listBuffer;
}

BufferView *View_GetBufferView(View *view){
    return &view->bufferView;
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
    if(buffer){
        SelectableList *list = &view->selectableList;
        *buffer = nullptr;
        if(i < list->used){
            uint id = list->selectable[i];
            AssertA(id < list->listBuffer->lineCount, "Invalid id translation");
            *buffer = LineBuffer_GetBufferAt(list->listBuffer, id);
        }
    }
}

void View_SelectableListSwapList(View *view, LineBuffer *sourceBuffer, int release){
    SelectableList *list = &view->selectableList;
    if(release){
        LineBuffer_Free(list->listBuffer);
        AllocatorFree(list->listBuffer);
    }
    
    View_SelectableListSetLineBuffer(view, sourceBuffer);
}

void View_SelectableListFilterBy(View *view, char *key, uint keylen){
    uint id = 0;
    SelectableList *list = &view->selectableList;
    if(key){
        FastStringSearchInit(key, keylen);
        
        for(uint i = 0; i < list->listBuffer->lineCount; i++){
            Buffer *buffer = LineBuffer_GetBufferAt(list->listBuffer, i);
            if(buffer->taken >= keylen){
                int fid = FastStringSearch(key, buffer->data, keylen, buffer->taken);
                if(fid >= 0){
                    list->selectable[id++] = i;
                }
            }
        }
        
    }else{
        for(uint i = 0; i < list->listBuffer->lineCount; i++){
            list->selectable[id++] = i;
        }
    }
    
    list->used = id;
    list->active = 0;
    View_SelectableListSetViewRange(view,
                                    vec2ui(0, Min(list->used,
                                                  list->currentLineRange)));
    if(list->used == 0) list->active = -1;
}

void View_SelectableListPush(View *view, char *item, uint len){
    if(len > 0){
        SelectableList *list = &view->selectableList;
        
        AssertA(view->state == View_SelectableList,
                "Invalid call to View_SelectableListPush");
        
        AssertA(list->selectable != nullptr && list->selectableSize > 0,
                "Invalid selectable list");
        
        uint insertId = list->used;
        if(!(insertId+1 < list->selectableSize)){
            uint n = insertId + 1 - list->selectableSize + DefaultAllocatorSize;
            list->selectable = AllocatorExpand(uint, list->selectable,
                                               n, list->selectableSize);
            list->selectableSize = n;
        }
        
        LineBuffer_InsertLine(list->listBuffer, item, len, 0);
        list->selectable[insertId] = list->listBuffer->lineCount-1;
        list->used++;
    }
}

void View_SelectableListSetViewRange(View *view, vec2ui range){
    SelectableList *list = &view->selectableList;
    list->viewRange = range;
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

void View_EarlyInitialize(View *view){
    view->descLocation = DescriptionTop; // TODO: Config
    view->selectableList = SELECTABLE_LIST_INITIALIZER;
    view->fileOpener = AllocatorGetN(FileOpener, 1);
    
    QueryBar_Initialize(&view->queryBar);
    
    view->renderList[View_FreeTyping] = {
        .stages = {
            Graphics_RenderView,
        },
        .stageCount = 1,
    };
    
    view->renderList[View_QueryBar] = {
        .stages = {
            Graphics_RenderView,
            Graphics_RenderQueryBar,
        },
        .stageCount = 2,
    };
    
    //TODO: Add render function
    view->renderList[View_SelectableList] = {
        .stages = {
            Graphics_RenderListSelector,
            Graphics_RenderQueryBar,
        },
        .stageCount = 2,
    };
}

void View_SetGeometry(View *view, Geometry geometry, uint lineHeight){
    Geometry barGeo;
    Float height = geometry.upper.y - geometry.lower.y;
    Float realHeight = kViewSelectableListScaling * lineHeight;
    Float pHeight = (1.0 + kViewSelectableListOffset) * realHeight;
    Float listLines = height / pHeight;
    uint count = (uint)Floor(listLines);
    uint displayable = (uint)Floor(height / realHeight) - 1;
    
    view->geometry = geometry;
    view->selectableList.currentLineRange = count;
    view->selectableList.currentDisplayRange = displayable;
    
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
    View_SelectableListSetViewRange(view, vec2ui(0, count));
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
    
    view->state = state;
}

int View_PreviousItem(View *view){
    if(view->state == View_QueryBar){
        return QueryBar_PreviousItem(&view->queryBar, view);
    }else if(view->state == View_SelectableList){
        SelectableList *list = &view->selectableList;
        if(list->active > -1){
            list->active--;
            if(list->active > 0)
                View_SelectableListUpdateRange(view);
        }else if(list->used > 0){
            list->active = list->used-1;
            View_SelectableListUpdateRange(view);
        }
    }
    
    return 0;
}

int View_NextItem(View *view){
    if(view->state == View_QueryBar){
        return QueryBar_NextItem(&view->queryBar, view);
    }else if(view->state == View_SelectableList){
        SelectableList *list = &view->selectableList;
        if(list->used > 0){
            if(list->active < (int)list->used-1){
                list->active++;
            }else{
                list->active = 0;
            }
            
            View_SelectableListUpdateRange(view);
        }
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
