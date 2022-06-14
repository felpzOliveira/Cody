#include <selectable.h>
#include <types.h>

static void SelectableListUpdateRange(SelectableList *list){
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

void SelectableList_SwapList(SelectableList *list, LineBuffer *sourceBuffer, int release){
    if(release){
        LineBuffer_Free(list->listBuffer);
        AllocatorFree(list->listBuffer);
    }

    SelectableList_SetLineBuffer(list, sourceBuffer);
}

LineBuffer *SelectableList_GetLineBuffer(SelectableList *list){
    return list->listBuffer;
}

int SelectableList_GetActiveIndex(SelectableList *list){
    return list->active;
}

uint SelectableList_GetRealIndex(SelectableList *list, uint i){
    uint r = 0;
    if(i < list->used){
        r = list->selectable[i];
    }

    return r;
}

void SelectableList_GetItem(SelectableList *list, uint i, Buffer **buffer){
    if(buffer){
        *buffer = nullptr;
        if(i < list->used){
            uint id = list->selectable[i];
            AssertA(id < list->listBuffer->lineCount, "Invalid id translation");
            *buffer = LineBuffer_GetBufferAt(list->listBuffer, id);
        }
    }
}

void SelectableList_Push(SelectableList *list, char *item, uint len){
    if(len > 0){
        AssertA(list->selectable != nullptr && list->selectableSize > 0,
                "Invalid selectable list");

        uint insertId = list->used;
        if(!(insertId+1 < list->selectableSize)){
            uint n = insertId + 1 - list->selectableSize + DefaultAllocatorSize;
            list->selectable = AllocatorExpand(uint, list->selectable,
                                               n, list->selectableSize);
            list->selectableSize = n;
        }

        LineBuffer_InsertLine(list->listBuffer, item, len);
        list->selectable[insertId] = list->listBuffer->lineCount-1;
        list->used++;
    }
}

void SelectableList_Filter(SelectableList *list, char *key, uint keylen){
    uint id = 0;
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
    SelectableList_SetRange(list, vec2ui(0, Min(list->used, list->currentLineRange)));
    if(list->used == 0) list->active = -1;
}

void SelectableList_SetItem(SelectableList *list, uint item){
    if(list->used > 0 && item < list->used){
        list->active = item;
        SelectableListUpdateRange(list);
    }
}

void SelectableList_NextItem(SelectableList *list){
    if(list->used > 0){
        if(list->active < (int)list->used-1){
            list->active++;
        }else{
            list->active = 0;
        }

        SelectableListUpdateRange(list);
    }
}

void SelectableList_PreviousItem(SelectableList *list, int minValue){
    if(list->active > minValue){
        list->active--;
        if(list->active > 0)
            SelectableListUpdateRange(list);
    }else if(list->used > 0){
        list->active = list->used-1;
        SelectableListUpdateRange(list);
    }
}

vec2ui SelectableList_GetViewRange(SelectableList *list){
    AssertA(list != nullptr, "Invalid list pointer");
    return vec2ui(list->viewRange.x, Min(list->viewRange.y, list->listBuffer->lineCount));
}

void SelectableList_Init(SelectableList *list){
    AssertA(list != nullptr, "Invalid list pointer");
    *list = SELECTABLE_LIST_INITIALIZER;
}

void SelectableList_SetRange(SelectableList *list, vec2ui range){
    AssertA(list != nullptr, "Invalid list pointer");
    list->viewRange = range;
}

void SelectableList_SetView(SelectableList *list, uint range, uint displayable){
    AssertA(list != nullptr, "Invalid list pointer");
    list->currentLineRange = range;
    list->currentDisplayRange = displayable;
}

void SelectableList_SetLineBuffer(SelectableList *list, LineBuffer *sourceBuffer){
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
    list->listBuffer = sourceBuffer;
}

void SelectableList_ResetView(SelectableList *list){
    AssertA(list != nullptr, "Invalid list pointer");
    list->viewRange = vec2ui(0, list->currentLineRange);
    list->active = 0;
}

void SelectableList_Set(SelectableList *list, LineBuffer *sourceBuffer){
    AssertA(list != nullptr, "Invalid list pointer");
    list->viewRange = vec2ui(0, list->currentLineRange);
    SelectableList_SetLineBuffer(list, sourceBuffer);
}
