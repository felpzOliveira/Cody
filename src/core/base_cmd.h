/* date = February 27th 2021 10:39 am */

#ifndef BASE_CMD_H
#define BASE_CMD_H
#include <types.h>
#include <geometry.h>
#include <functional>

struct QueryBar;
struct LineBuffer;
struct FileOpener;
struct View;
struct FileEntry;

#define OnFileOpenCallback std::function<void(View *view, FileEntry *entry)>
#define OnInteractiveSearch std::function<int(QueryBar *bar, View *view, int accepted)>

/* Commands are pre-defined by their enum id */
typedef enum{
    QUERY_BAR_CMD_NONE = 0,
    QUERY_BAR_CMD_SEARCH,
    QUERY_BAR_CMD_REVERSE_SEARCH,
    QUERY_BAR_CMD_JUMP_TO_LINE,
    QUERY_BAR_CMD_SEARCH_AND_REPLACE,

    QUERY_BAR_CMD_CUSTOM,
}QueryBarCommand;

typedef enum{
    QUERY_BAR_SEARCH_AND_REPLACE_SEARCH = 0,
    QUERY_BAR_SEARCH_AND_REPLACE_REPLACE,
    QUERY_BAR_SEARCH_AND_REPLACE_EXECUTE,
    QUERY_BAR_SEARCH_AND_REPLACE_ASK,
}QueryBarSearchAndReplaceState;

typedef struct QueryBarCmdSearch{
    uint lineNo;
    uint position;
    uint length;
    short valid;
}QueryBarCmdSearch;

typedef struct QueryBarCmdSearchAndReplace{
    QueryBarSearchAndReplaceState state;
    char toLocate[128];
    uint toLocateLen;

    char toReplace[128];
    uint toReplaceLen;

    /* Callback when user confirms/denies a search value */
    OnInteractiveSearch searchCallback;
}QueryBarCmdSearchAndReplace;

/*
* Performs one computation of the search operation based on the contents
* of the QueryBar using a LineBuffer as a search environment using a cursor
* as a starting point. Returns 1 in case a new match was found, 0 otherwise.
*/
int QueryBarCommandSearch(QueryBar *queryBar, LineBuffer *lineBuffer,
                          DoubleCursor *dcursor, char *toSearch = nullptr,
                          uint toSearchLen = 0);



/*
* Perform setup to start a command of file opening.
*/
int FileOpenerCommandStart(View *view, char *basePath, ushort len,
                           OnFileOpenCallback onOpenFile);
/*
* Performs setup to start a command of buffer switch.
*/
int SwitchBufferCommandStart(View *view);

/*
* Performs setup to start a command theme.
*/
int SwitchThemeCommandStart(View *view);

/*
* Interpret and execute a given command applied to a given view.
*/
int BaseCommand_Interpret(char *cmd, uint size, View *view);

/*
* Pushes a new filename, existing or not, into the config for auto-loading.
*/
int BaseCommand_AddFileEntryIntoAutoLoader(char *entry, uint size);

#endif //BASE_CMD_H
