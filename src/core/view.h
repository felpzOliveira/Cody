/* date = February 25th 2021 7:41 pm */

#ifndef VIEW_H
#define VIEW_H
#include <bufferview.h>
#include <graphics.h>
#include <utilities.h>
#include <query_bar.h>

#define View_IsQueryBarActive(view) ((view->state) == View_QueryBar || (view->state) == View_SelectableList)

#define SELECTABLE_LIST_INITIALIZER { .listBuffer = nullptr, .selectable = nullptr, .selectableSize = 0, .used = 0, }

typedef enum{
    View_FreeTyping = 0,
    View_QueryBar,
    View_SelectableList,
    View_StatesCount
}ViewState;

struct FileOpener{
    FileEntry *entries;
    char basePath[PATH_MAX];
    uint entryCount;
    uint entrySize;
    ushort pathLen;
};

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

struct View{
    BufferView bufferView;
    FileOpener *fileOpener;
    SelectableList selectableList;
    QueryBar queryBar;
    Geometry geometry;
    DescriptionLocation descLocation;
    int isActive;
    ViewState state;
    RenderList renderList[View_StatesCount];
};

/*
* Gets the top-most, i.e.: default, state of a view.
*/
ViewState View_GetDefaultState();

/*
* Initializes a view component.
*/
void View_EarlyInitialize(View *view);

/*
* Sets the geometry of the view component.
*/
void View_SetGeometry(View *view, Geometry geometry, uint lineHeight);

/*
* Sets the view as the active view.
*/
void View_SetActive(View *view, int isActive);

/*
* Triggers a view to be in a selectable list mode.
*/
void View_SelectableListSet(View *view, LineBuffer *sourceBuffer,
                            char *title, uint titlelen, OnQueryBarEntry entry,
                            OnQueryBarCancel cancel, OnQueryBarCommit commit,
                            QueryBarInputFilter *filter);

/*
* Gets the view range of a views selectable list.
*/
vec2ui View_SelectableListGetViewRange(View *view);

/*
* Sets the default view range length for a views selectable list.
*/
void View_SelectableListSetViewRange(View *view, vec2ui range);

/*
* Swaps the active linebuffer in a views selectable list, you can use the 'release'
* flag if the view should automatically claim the resources of the current linebuffer.
*/
void View_SelectableListSwapList(View *view, LineBuffer *sourceBuffer, int release=1);

/*
* Returns the LineBuffer in use by a views selectable list.
*/
LineBuffer *View_SelectableListGetLineBuffer(View *view);

/*
* Pushes a new item into a view configured as a SelectableList, this function
* does not trigger any callback set with 'View_SelectableListSet'.
*/
void View_SelectableListPush(View *view, char *item, uint len);

/*
* Filters the views selectable list by a key value.
*/
void View_SelectableListFilterBy(View *view, char *key, uint keylen);

/*
* Gets a reference to the Buffer located at index 'i' inside the views selectable list.
*/
void View_SelectableListGetItem(View *view, uint i, Buffer **buffer);

/*
* Retrieves the active index of a views selectable list.
*/
int View_SelectableListGetActiveIndex(View *view);

/*
* Returns the real index of the _current_ visible 'ith' element.
*/
uint View_SelectableListGetRealIndex(View *view, uint i);

/*
* Triggers the view to jump to the next item if its state is >= View_QueryBar.
*/
int View_NextItem(View *view);

/*
* Triggers the view to jump to the previous item if its state is >= View_QueryBar.
*/
int View_PreviousItem(View *view);

/*
* Gets the rendering pipeline for the current view state.
*/
RenderList *View_GetRenderPipeline(View *view);

/*
* Gets the current state of a view.
*/
ViewState View_GetState(View *view);

/*
* Forces a view to return to a state discarding any pending changes.
*/
void View_ReturnToState(View *view, ViewState state);

/*
* Forces a view to return to a state applying any pending changes.
*/
int View_CommitToState(View *view, ViewState state);

/*
* Gets the bufferview from a view.
*/
BufferView *View_GetBufferView(View *view);

/*
* Gets the QueryBar of a view.
*/
QueryBar *View_GetQueryBar(View *view);

/*
* Gets the FileOpener of a view.
*/
FileOpener *View_GetFileOpener(View *view);

/*
* Release resources taken by a view.
*/
void View_Free(View *view);


/* Debug stuff */
void View_SelectableListDebug(View *view);
#endif //VIEW_H