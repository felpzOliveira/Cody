/* date = February 7th 2021 4:30 pm */

#ifndef QUERY_BAR_H
#define QUERY_BAR_H
#include <base_cmd.h>
#include <buffers.h>
#include <geometry.h>
#include <keyboard.h>
#include <functional>
#include <vector>

#define OnQueryBarEntry  std::function<int(QueryBar *, View*)>
#define OnQueryBarCancel std::function<int(QueryBar *, View*)>
#define OnQueryBarCommit std::function<int(QueryBar *, View*)>

#define INPUT_FILTER_INITIALIZER { .digitOnly = 0, .allowFreeType = 1, .allowCursorJump = false, }

#define QUERY_BAR_DEFAULT_JUMP_STR     " ,/"
#define QUERY_BAR_DEFAULT_JUMP_STR_LEN 3

/* Query bar is also the logical FileBar */

struct View;
struct QueryBar;

// This should probably be a regex, but I'm gonna avoid it for now
struct QueryBarInputFilter{
    int digitOnly;
    int allowFreeType;
    bool allowCursorJump;
    bool toHistory;
};

struct QueryBarHistoryItem{
    std::string value;
};

struct QueryBarHistory{
    CircularStack<QueryBarHistoryItem> *history;
};

struct QueryBar{
    Geometry geometry;
    Buffer buffer;
    DoubleCursor cursor;
    vec2ui cursorBackup;
    int isActive;
    uint writePos;
    uint writePosU8;
    QueryBarCommand cmd;
    QueryBarInputFilter filter;
    int qHistoryAt;

    /* Command related data */
    QueryBarCmdSearch searchCmd;
    QueryBarCmdSearchAndReplace replaceCmd;

    /* For custom usage */
    OnQueryBarEntry  entryCallback; // happens when the query bar buffer changes
    OnQueryBarCancel cancelCallback; // when user presses Esc on a query bar
    OnQueryBarCommit commitCallback; // when user presses Enter on a query bar

    /* For file opening, this is called when user confirms a file */
    OnFileOpenCallback fileOpenCallback;
};

/*
* Initializes a QueryBar.
*/
void QueryBar_Initialize(QueryBar *queryBar);

/*
* Sets the geometry of the QueryBar.
*/
void QueryBar_SetGeometry(QueryBar *queryBar, Geometry *geometry, Float lineHeight);

/*
* Activates a QueryBar and prepares it to execute a given cmd.
*/
void QueryBar_Activate(QueryBar *queryBar, QueryBarCommand cmd, View *view);

/*
* Activates a QueryBar with a custom interface for controling events.
* Reset states must be made manually when custom commands are used.
* Callbacks returns inform if querybar should continue automatic processing
* of the event. Return values of 0 mean to **not** continue processing and
* keep the state unchanged, >= 0 values will trigger default behaviour, while
* negative values inform that an error occur and state should be reverted immediatly.
*/
void QueryBar_ActivateCustom(QueryBar *queryBar, char *title, uint titlelen,
                             OnQueryBarEntry entry, OnQueryBarCancel cancel,
                             OnQueryBarCommit commit, QueryBarInputFilter *filter);

/*
* Same as QueryBar_ActivateCustom but allow you to set the cmd id. Be aware that
* the id must be solvable by the query bar internal working.
*/
void QueryBar_ActiveCustomFull(QueryBar *queryBar, char *title, uint titlelen,
                               OnQueryBarEntry entry, OnQueryBarCancel cancel,
                               OnQueryBarCommit commit, QueryBarInputFilter *filter,
                               QueryBarCommand cmd);
/*
* Move the cursor left.
*/
void QueryBar_MoveLeft(QueryBar *queryBar);

/*
* Move the cursor right.
*/
void QueryBar_MoveRight(QueryBar *queryBar);

/*
* Move the cursor to the previous default character similarly to the editor cursor
* auto jumping.
*/
void QueryBar_JumpToPrevious(QueryBar *queryBar);

/*
* Move the cursor to the next default character similarly to the editor cursor
* auto jumping.
*/
void QueryBar_JumpToNext(QueryBar *queryBar);

/*
* Calls that want its content to be cached to the query bar history
* must do so manually.
*/
void QueryBar_EnableInputToHistory(QueryBar *queryBar);

/*
* Calls that want the cursor to be dynamic and change locations must
* call this routine.
*/
void QueryBar_EnableCursorJump(QueryBar *queryBar);

/*
* Returns a accessor to the data written into a QueryBar after title
* passed during activation. Length 'len' is returned in bytes.
* NOTE: DO NOT free the pointer returned as it points to internal data.
*/
void QueryBar_GetWrittenContent(QueryBar *queryBar, char **ptr, uint *len);

/*
* Returns a accessor to the current title of a QueryBar. Length 'len' is returned
* in bytes.
*/
void QueryBar_GetTitle(QueryBar *queryBar, char **ptr, uint *len);

/*
* Inserts a string into the a QueryBar. Returns 1 in case a change was made
* into the view according to the active command, 0 otherwise.
*/
int QueryBar_AddEntry(QueryBar *queryBar, View *view, char *str, uint len);

/*
* Sets the querybar content to be the one provided in 'str'.
* Warning: This function erases the current content of the querybar and 
* does not trigger any pending callbacks registered with 'QueryBar_ActivateCustom'.
*/
void QueryBar_SetEntry(QueryBar *queryBar, View *view, char *str, uint len);

/*
* Erases the last character of a QueryBar.
*/
int QueryBar_RemoveOne(QueryBar *queryBar, View *view);

/*
* Erases all input behind the cursor (not the tittle) of a QueryBar.
*/
void QueryBar_RemoveAllFromCursor(QueryBar *queryBar);

/*
* Triggers the query bar to go to the next logical item. Returns 1 in case a 
* change was made into the view according to the active command, 0 otherwise.
*/
int QueryBar_NextItem(QueryBar *queryBar, View *view);

/*
* Triggers the query bar to go to the previous logical item. Returns 1 in case a 
* change was made into the view according to the active command, 0 otherwise.
*/
int QueryBar_PreviousItem(QueryBar *queryBar, View *view);

/*
* Gets a reference to the results of the previously executed search command.
*/
void QueryBar_GetSearchResult(QueryBar *queryBar, QueryBarCmdSearch **result);

/*
* Sets the default callback for the interactive search to be called when user
* confirms/denies a search value.
*/
void QueryBar_SetInteractiveSearchCallback(QueryBar *queryBar,
                                           OnInteractiveSearch onConfirm);

/*
* Gets the active command in a QueryBar, if any.
*/
QueryBarCommand QueryBar_GetActiveCommand(QueryBar *queryBar);

/*
* Resets a QueryBar and apply pending changes depending on the 'commit' flag.
*/
int QueryBar_Reset(QueryBar *queryBar, View *view, int commit);

/*
* Releases resources taken by a QueryBar.
*/
void QueryBar_Free(QueryBar *queryBar);

/*
* Loads the query bar history detached.
*/
void QueryBarHistory_DetachedLoad(QueryBarHistory *qHistory, const char *basePath);

/*
* Store the query bar history detached.
*/
void QueryBarHistory_DetachedStore(QueryBarHistory *qHistory, const char *basePath);

/*
* Gets the path for the history filename.
*/
std::string QueryBarHistory_GetPath();

#endif //QUERY_BAR_H
