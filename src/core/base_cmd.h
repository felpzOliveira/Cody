/* date = February 27th 2021 10:39 am */

#ifndef BASE_CMD_H
#define BASE_CMD_H
#include <types.h>
#include <geometry.h>
#include <functional>
#include <string>

struct QueryBar;
struct LineBuffer;
struct FileOpener;
struct View;
struct FileEntry;

#define OnFileOpenCallback std::function<void(View *view, FileEntry *entry)>
#define OnInteractiveSearch std::function<int(QueryBar *bar, View *view, int accepted)>

#define CMD_CURSORSET_STR "cursor-format "
#define CMD_DIMM_STR "dimm "
#define CMD_KILLSPACES_STR "kill-spaces"
#define CMD_SEARCH_STR "search "
#define CMD_FUNCTIONS_STR "functions"
#define CMD_GIT_STR "git "
#define CMD_HSPLIT_STR "hsplit"
#define CMD_VSPLIT_STR "vsplit"
#define CMD_EXPAND_STR "expand"
#define CMD_KILLVIEW_STR "kill-view"
#define CMD_KILLBUFFER_STR "kill-buffer"
#define CMD_CURSORSEG_STR "cursor-seg "
#define CMD_DBG_START_STR "dbg start "
#define CMD_DBG_BREAK_STR "dbg break "
#define CMD_DBG_EXIT_STR "dbg exit"
#define CMD_DBG_RUN_STR "dbg run"
#define CMD_DBG_FINISH_STR "dbg finish"
#define CMD_DBG_EVALUATE_STR "dbg eval "

#define MAX_THREADS 16
#define MAX_SEARCH_ENTRIES 2048
#define MAX_QUERIABLE_BUFFERS 256
struct GlobalSearchResult{
    LineBuffer *lineBuffer;
    int line;
    int col;
};

struct GlobalSearch{
    GlobalSearchResult results[MAX_SEARCH_ENTRIES];
    uint count;
};

/* Commands are pre-defined by their enum id */
typedef enum{
    QUERY_BAR_CMD_NONE = 0,
    QUERY_BAR_CMD_SEARCH,
    QUERY_BAR_CMD_REVERSE_SEARCH,
    QUERY_BAR_CMD_JUMP_TO_LINE,
    QUERY_BAR_CMD_SEARCH_AND_REPLACE,
    QUERY_BAR_CMD_INTERACTIVE,
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
* Initializes the command map.
*/
void BaseCommand_InitializeCommandMap();

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

/*
* Gets the current global search result. You are not allowed to touch this data,
* it is only available for reading in order to render or interact with user.
* It returns the amount of entries in the search list, this value is hardcoded
* to MAX_THREADS, unless it is not possible to return the value, in which case
* 0 is returned instead. In order to inspect how many entries are available it
* is required to loop the list and check each individual 'count' value.
*/
uint BaseCommand_FetchGlobalSearchData(GlobalSearch **gSearch);

/*
* Searches for a functions, exposing for app to be able
* to implement 'AppCommandListFunctions'.
*/
int BaseCommand_SearchFunctions(char *cmd, uint size, View *);

#endif //BASE_CMD_H
