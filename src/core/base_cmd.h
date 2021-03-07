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

/* Commands are pre-defined by their enum id */
typedef enum{
    QUERY_BAR_CMD_NONE = 0,
    QUERY_BAR_CMD_SEARCH,
    QUERY_BAR_CMD_REVERSE_SEARCH,
    QUERY_BAR_CMD_JUMP_TO_LINE,
    
    QUERY_BAR_CMD_CUSTOM,
}QueryBarCommand;

struct QueryBarCmdSearch{
    uint lineNo;
    uint position;
    uint length;
    short valid;
};

/*
* Performs one computation of the search operation based on the contents
* of the QueryBar using a LineBuffer as a search environment using a cursor
* as a starting point. Returns 1 in case a new match was found, 0 otherwise.
*/
int QueryBarCommandSearch(QueryBar *queryBar, LineBuffer *lineBuffer,
                          DoubleCursor *cursor);
/*
* Perform setup to start a command of file opening.
*/
int FileOpenerCommandStart(View *view, char *basePath, ushort len,
                           OnFileOpenCallback onOpenFile);

int SwitchBufferCommandStart(View *view);

#endif //BASE_CMD_H
