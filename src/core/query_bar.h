/* date = February 7th 2021 4:30 pm */

#ifndef QUERY_BAR_H
#define QUERY_BAR_H
#include <base_cmd.h>
#include <buffers.h>
#include <geometry.h>
#include <keyboard.h>
#include <base_cmd.h>
#include <functional>

#define OnQueryBarEntry  std::function<int(QueryBar *, View*)>
#define OnQueryBarCancel std::function<int(QueryBar *, View*)>
#define OnQueryBarCommit std::function<int(QueryBar *, View*)>

#define INPUT_FILTER_INITIALIZER { .digitOnly = 0, .allowFreeType = 1, }

/* Query bar is also the logical FileBar */

struct View;
struct QueryBar;

// This should probably be a regex, but I'm gonna avoid it for now
struct QueryBarInputFilter{
    int digitOnly;
    int allowFreeType;
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
    
    /* Command related data */
    QueryBarCmdSearch searchCmd;
    
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
* Move the cursor left.
*/
void QueryBar_MoveLeft(QueryBar *queryBar);

/*
* Move the cursor right.
*/
void QueryBar_MoveRight(QueryBar *queryBar);

/*
* Returns a accessor to the data written into a QueryBar after title
* passed during activation. Length 'len' is returned in bytes.
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

#endif //QUERY_BAR_H
