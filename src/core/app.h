/* date = February 3rd 2021 3:45 pm */

#ifndef APP_H
#define APP_H
#include <geometry.h>
#include <view.h>
#include <file_buffer.h>

struct BufferView;
struct Buffer;

typedef enum{
    CURSOR_RECT,
    CURSOR_DASH,
}CursorStyle;

typedef struct{
    int tabSpacing;
    int useTabs;
    CursorStyle cStyle;
}AppConfig;

extern AppConfig appGlobalConfig;

/*
* Performs early initialization of App structures when there is no graphics
* interface loaded.
*/
void AppEarlyInitialize();

/*
* Performs initialization when there graphics interface is already loaded.
*/
void AppInitialize();

/*
* Inserts a new linebuffer into the apps buffer list.
*/
void AppInsertLineBuffer(LineBuffer *lineBuffer);

/*
* Checks if the app already loaded the file given in path.
*/
int AppIsFileLoaded(char *path, uint len);

/*
* Gets apps file buffer list.
*/
FileBufferList *AppGetFileBufferList();

/*
* Gets the bufferview from the active View.
*/
BufferView *AppGetActiveBufferView();

/*
* Triggers the view at (x,y) to be active.
*/
vec2ui AppActivateViewAt(int x, int y);

/*
* Gets the view located at (x,y).
*/
View *AppGetViewAt(int x, int y);

/*
* Update all views to follow a given geometry configuration.
*/
void AppSetViewingGeometry(Geometry geometry, Float lineHeight);

/*
* Gets the active View.
*/
View *AppGetActiveView();

/*
* Sets a specific view to be active.
*/
void AppSetActiveView(int i);

/*
* Get the bufferview from the view index i.
*/
BufferView *AppGetBufferView(int i);

/*
* Get the view with index i.
*/
View *AppGetView(int i);

/*
* Get the amount of views that exist.
*/
int AppGetViewCount();

/*
* Gets cwd from app context.
*/
char *AppGetContextDirectory();

/*
* Computes a indentation level of a line.
*/
uint AppComputeLineIndentLevel(Buffer *buffer);

/*
* Computes the indentation level of the line given by buffer+1.
* This is slower than 'AppComputeLineIndentLevel' but can be used to
* generate new lines when the line buffer+1 has not yet been constructed.
*/
uint AppComputeLineLastIndentLevel(Buffer *buffer);

/*
* Allow views to synchronize their properties.
*/
void AppUpdateViews();

/*
* Inserts a UTF-8 string into the current active view.
*/
void AppDefaultEntry(char *utf8Data, int utf8Size);

/*
* Forces the active view to return to its default state.
*/
void AppDefaultReturn();

/*
* Removes one character from the current active view.
*/
void AppDefaultRemoveOne();

/* Base commands for free typing */
void AppCommandJumpLeftArrow();
void AppCommandJumpRightArrow();
void AppCommandJumpUpArrow();
void AppCommandJumpDownArrow();
void AppCommandLeftArrow();
void AppCommandRightArrow();
void AppCommandUpArrow();
void AppCommandDownArrow();
void AppCommandIndent();
void AppCommandPaste();
void AppCommandCut();
void AppCommandJumpNesting();
void AppCommandCopy();
void AppCommandSaveBufferView();
void AppCommandLineQuicklyDisplay();
void AppCommandSwapLineNbs();
void AppCommandSetGhostCursor();
void AppCommandSwapView();
void AppCommandEndLine();
void AppCommandHomeLine();
void AppCommandUndo();
void AppCommandNewLine();
void AppCommandInsertTab();
void AppCommandRemovePreviousToken();
void AppCommandQueryBarSearch();
void AppCommandQueryBarRevSearch();
void AppCommandQueryBarGotoLine();
void AppCommandIndentRegion(BufferView *view, vec2ui start, vec2ui end);
void AppCommandRemoveTextBlock(BufferView *bufferView, vec2ui start, vec2ui end);

/* Base commands for the query bar */
void AppCommandQueryBarNext();
void AppCommandQueryBarPrevious();
void AppCommandQueryBarRemoveAll();

#endif //APP_H
