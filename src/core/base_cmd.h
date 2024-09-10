/* date = February 27th 2021 10:39 am */

#ifndef BASE_CMD_H
#define BASE_CMD_H
#include <types.h>
#include <geometry.h>
#include <functional>
#include <string>
#include <vector>

struct QueryBar;
struct LineBuffer;
struct FileOpener;
struct View;
struct FileEntry;

#define OnFileOpenCallback std::function<void(View *view, FileEntry *entry)>
#define OnInteractiveSearch std::function<int(QueryBar *bar, View *view, int accepted)>

#define CMD_CHANGE_FONTSIZE_STR "fontsize"
#define CMD_CHANGE_FONTSIZE_HELP "Sets the default font size to be used."

#define CMD_CHANGE_FONT_STR "font"
#define CMD_CHANGE_FONT_HELP "Change the default font being used."

#define CMD_INDENT_FILE_STR "indent-file"
#define CMD_INDENT_FILE_HELP "(Re)Indent the current opened file."

#define CMD_INDENT_REGION_STR "indent-region"
#define CMD_INDENT_REGION_HELP "(Re)Indent the current region defined by the cursor."

#define CMD_CHANGE_CONTRAST_STR "contrast"
#define CMD_CHANGE_CONTRAST_HELP "Sets the constrast value for the current theme."

#define CMD_PUT_STR "put"
#define CMD_PUT_HELP "Send a file to the RPC server."

#define CMD_GET_STR "get"
#define CMD_GET_HELP "Get a file to the RPC server."

#define CMD_CHANGE_BRIGHTNESS_STR "brightness"
#define CMD_CHANGE_BRIGHTNESS_HELP "Sets the brightness value for the current theme."

#define CMD_CHANGE_SATURATION_STR "saturation"
#define CMD_CHANGE_SATURATION_HELP "Sets the saturation value for the current theme."

#define CMD_WRONGIDENT_DISPLAY_STR "ident-display"
#define CMD_WRONGIDENT_DISPLAY_HELP "Toogle the display of the wrong identation character."

#define CMD_CURSORSET_STR "cursor-format "
#define CMD_CURSORSET_HELP "Sets the format of the cursor (choices: quad, dash, rect)."

#define CMD_DIMM_STR "dimm "
#define CMD_DIMM_HELP "Sets if the current theme should dimm inactive views (choices: on/off)."

#define CMD_KILLSPACES_STR "kill-spaces"
#define CMD_KILLSPACES_HELP "Removes all extra spaces in the current file."

#define CMD_SEARCH_STR "search "
#define CMD_SEARCH_HELP "Perform a full search on all opened files (usage: search <value>)."

#define CMD_ENCODING_STR "encoding"
#define CMD_ENCODING_HELP "Changes the encoding for the given file"

#define CMD_GLOBAL_ENCODING_STR "set-global-encoding"
#define CMD_GLOBAL_ENCODING_HELP "Change the global encoding sets for default buffers."

#define CMD_FUNCTIONS_STR "functions"
#define CMD_FUNCTIONS_HELP "List all functions found on all opened files."

#define CMD_HSPLIT_STR "hsplit"
#define CMD_HSPLIT_HELP "Split the current view horizontally."

#define CMD_CURSOR_BLINK_STR "cursor-blink"
#define CMD_CURSOR_BLINK_HELP "Toogles cursor blinking"

#define CMD_VSPLIT_STR "vsplit"
#define CMD_VSPLIT_HELP "Split the current view vertically."

#define CMD_EXPAND_STR "expand"
#define CMD_EXPAND_HELP "Expand/Restore a view to the largest possible size."

#define CMD_SET_ENCRYPT_STR "set-encrypted"
#define CMD_SET_ENCRYPT_HELP "Marks the current file to be encrypted."

#define CMD_KILLVIEW_STR "kill-view"
#define CMD_KILLVIEW_HELP "Closes a view reclaiming the view's taken geometry."

#define CMD_KILLBUFFER_STR "kill-buffer"
#define CMD_KILLBUFFER_HELP "Closes the file opened in the current view (if any)."

#define CMD_HISTORY_CLEAR_STR "history-clear"
#define CMD_HISTORY_CLEAR_HELP "Clears the current history stack and file."

#define CMD_CURSORSEG_STR "cursor-seg "
#define CMD_CURSORSEG_HELP "Enable/Disable rendering of the line that connects the double cursor."

#define CMD_PATH_COMPRESSION_STR "path-compression "
#define CMD_PATH_COMPRESSION_HELP "Configures the amount by which paths should be reduced in the query bar."

#define CMD_JUMP_TO_STR "goto"
#define CMD_JUMP_TO_HELP "Jump to a specific part of the current view."

#define CMD_DBG_START_STR "dbg start "
#define CMD_DBG_START_HELP "Start the debugger on a given file (unfinished)."

#define CMD_DBG_BREAK_STR "dbg break "
#define CMD_DBG_BREAK_HELP "Adds a breakpoint on a given instance of the debugger (unfinished)."

#define CMD_DBG_EXIT_STR "dbg exit"
#define CMD_DBG_EXIT_HELP "Terminates the debugger (unfinished)."

#define CMD_DBG_RUN_STR "dbg run"
#define CMD_DBG_RUN_HELP "Triggers execution of the program opened in the debugger (unfinished)."

#define CMD_DBG_FINISH_STR "dbg finish"
#define CMD_DBG_FINISH_HELP "Sends a finish command to the debuggee (unfinished)."

#define CMD_DBG_EVALUATE_STR "dbg eval "
#define CMD_DBG_EVALUATE_HELP "Evaluates an expression on an instance of the debugger (unfinished)."

#define MAX_THREADS 16
struct GlobalSearchResult{
    LineBuffer *lineBuffer;
    uint line;
    uint col;
};

struct GlobalSearch{
    std::vector<GlobalSearchResult> results;
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
* Performs setup to start a command for theme swap.
*/
int SwitchThemeCommandStart(View *view);

/*
* Performs setup to start a command for font swap.
*/
int SwitchFontCommandStart(View *view);

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
