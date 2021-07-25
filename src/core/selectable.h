#if !defined(SELECTABLE_H)
#define SELECTABLE_H
#include <buffers.h>

#define SELECTABLE_LIST_INITIALIZER { .listBuffer = nullptr, .viewRange = vec2ui(), .selectable = nullptr, .selectableSize = 0, .used = 0, .currentLineRange = 0, .currentDisplayRange = 0, .active = 0}

struct SelectableList{
    LineBuffer *listBuffer;
    vec2ui viewRange;
    uint *selectable;
    uint selectableSize;
    uint used;
    uint currentLineRange;
    uint currentDisplayRange;
    int active;
};

void SelectableList_Init(SelectableList *list);

void SelectableList_Set(SelectableList *list, LineBuffer *sourceBuffer);

void SelectableList_SetView(SelectableList *list, uint range, uint displayable);

void SelectableList_SetRange(SelectableList *list, vec2ui range);

void SelectableList_SetLineBuffer(SelectableList *list, LineBuffer *sourceBuffer);

LineBuffer *SelectableList_GetLineBuffer(SelectableList *list);

vec2ui SelectableList_GetViewRange(SelectableList *list);

void SelectableList_NextItem(SelectableList *list);

void SelectableList_PreviousItem(SelectableList *list, int minValue);

void SelectableList_Filter(SelectableList *list, char *key, uint keylen);

void SelectableList_Push(SelectableList *list, char *item, uint len);

void SelectableList_GetItem(SelectableList *list, uint i, Buffer **buffer);

void SelectableList_SwapList(SelectableList *list, LineBuffer *souceBuffer, int release);

int SelectableList_GetActiveIndex(SelectableList *list);

uint SelectableList_GetRealIndex(SelectableList *list, uint i);

void SelectableList_ResetView(SelectableList *list);

#endif // SELECTABLE_H
