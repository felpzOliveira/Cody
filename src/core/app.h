/* date = February 3rd 2021 3:45 pm */

#ifndef APP_H
#define APP_H
#include <geometry.h>
#include <view.h>
#include <file_buffer.h>
#include <vector>
#include <view_tree.h>
#include <dbgapp.h>

struct BufferView;
struct Buffer;

typedef enum{
    CURSOR_RECT,
    CURSOR_DASH,
    CURSOR_QUAD,
}CursorStyle;

typedef struct{
    int tabSpacing;
    int useTabs;
    int autoCompleteSize;
    std::string rootFolder;
    std::string configFile;
    std::string configFolder;
    std::vector<std::string> filesStored;
    CursorStyle cStyle;
}AppConfig;

extern AppConfig appGlobalConfig;

/*
* Performs early initialization of App structures when there is no graphics
* interface loaded.
*/
void AppEarlyInitialize();

/*
* Performs initialization when the graphics interface is already loaded.
*/
void AppInitialize();

/*
* Gets the bufferview from the active View.
*/
BufferView *AppGetActiveBufferView();

BindingMap *AppGetFreetypingBinding();

/*
* Gets tab configuration. Returns the tab spacing and 'using_tab' returns
* 1 in case the editor is using tabs or 0 in case it is offseting with spaces.
*/
int AppGetTabConfiguration(int *using_tab);

/*
* Sets the style for rendering the cursor.
*/
void AppSetCursorStyle(CursorStyle style);

/*
* Gets the style for rendering the cursor.
*/
CursorStyle AppGetCursorStyle();

/*
* Triggers the view at (x,y) to be active. Variable force_binding
* is the same from 'AppSetActiveView'.
*/
vec2ui AppActivateViewAt(int x, int y, bool force_binding=true);

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
* Gets the global history of the querybar.
*/
QueryBarHistory *AppGetQueryBarHistory();

/*
* Search for the view that holds an accessor to the given linebuffer.
* Considerations:
*    1 - If no view is currently displaying the linebuffer than this routine
*        returns nullptr;
*    2 - If more than one view is currently displaying the linebuffer than
*        the first one found in the view tree is returned;
*    3 - Comparation is performed by pointer and not by file path, the linebuffer
*        must be the exact one being searched. This allows for non-file searches.
*/
View *AppSearchView(LineBuffer *lineBuffer);

/*
* Sets a specific view to be active. The force_binding variable can be used
* to define if the state should make sure the mappings are correct for it. This
* usually is true, unless you are calling this from an event and needs to persist
* the state for the event, i.e.: dbg.
*/
void AppSetActiveView(View *view, bool force_binding=true);

/*
* Forces the current view to swap to the binding for the given state.
*/
void AppSetBindingsForState(ViewState state, ViewType type=NoneView);

/*
* Gets cwd from app context.
*/
char *AppGetContextDirectory();

/*
* Computes the indentation level of the line given by buffer+1.
* This is slower than 'AppComputeLineIndentLevel' but can be used to
* generate new lines when the line buffer+1 has not yet been constructed.
*/
uint AppComputeLineLastIndentLevel(Buffer *buffer);

/*
* Returns an accessor to the next view node in the view node tree if possible.
*/
ViewNode *AppGetNextViewNode();

/*
* Computes a indentation level of a line until a UTF-8 position 'p'.
*/
uint AppComputeLineIndentLevel(Buffer *buffer, uint p);

/*
* Computes a indentation level of a line until the first non-empty character.
*/
uint AppComputeLineIndentLevel(Buffer *buffer);

/*
* Allow views to synchronize their properties.
*/
void AppUpdateViews();

/*
* Inserts a UTF-8 string into the current active view.
*/
void AppDefaultEntry(char *utf8Data, int utf8Size, void *);

/*
* Forces the active view to return to its default state.
*/
void AppDefaultReturn();

/*
* Removes one character from the current active view.
*/
void AppDefaultRemoveOne();

/*
* Gets the current geometry configured being used.
*/
Geometry AppGetScreenGeometry(Float *lineHeight);

/*
* Gets path related to the global configuration file.
*/
std::string AppGetConfigFilePath();
std::string AppGetConfigDirectory();
std::string AppGetRootDirectory();
bool AppIsPathFromRoot(std::string path);

/*
* Adds a file path into the list of stored paths.
*/
void AppAddStoredFile(std::string basePath);

/*
* Loop each breakpoint of the debugger that is on the given file path.
*/
void DbgSupport_ForEachBkpt(std::string path, std::function<void(DbgBkpt *)> callback);

/*
* Clears the breakpoint map that is used for rendering purposes only.
*/
void DbgSupport_ResetBreakpointMap();

/*
* Checks if a path given in relation to the root directory is already
* present in the stored files list.
*/
int AppIsStoredFile(std::string path);

/*
* Inserts string 'p' with size 'size' in the current active view
* with the modifier for paste. 'force_view' can be used to directly
* write to the active linebuffer even if the view is not in a typing
* state. This is helpfull in case the view is in querybar state and
* we would like to paste to the view instead of the query bar.
*/
void AppPasteString(const char *p, uint size, bool force_view=false);

/*
* Handles a scroll to a new position. x and y hold the new mouse coordinates
* in windows coordinates, state is the **global** opengl state for rendering requirements,
* height is the current height of the window and is_up tells which direction
* the scroll is going.
*/
void AppHandleMouseScroll(int x, int y, int is_up, OpenGLState *state);

/*
* Handles the mouse press event. Receives the mouse coordinates in x and y in window
* coordinates, the **global** opengl state fore rendering. It uses the press event
* to handle cursor movement while in code view.
*/
void AppHandleMousePress(int x, int y, OpenGLState *state);

/*
* Handles the mouse click event. It uses this event when the view is using a selectable
* list mode, i.e.: listing files, buffers, ...
*/
void AppHandleMouseClick(int x, int y, OpenGLState *state);

/*
* Handles the mouse double click event. This handler is mostly for external handlers,
* i.e.: debugger, popups, ... the app itself does not change states based on it. It
* is mostly for simplifying actions and not requiring a full typed query bar whenever
* we can deduce what needs to be done.
*/
void AppHandleDoubleClick(int x, int y, OpenGLState *state);

/*
* Handles the mouse motion event. Receives the current mouse coordinates after motion,
* this coordinates are in **global** values.
*/
void AppHandleMouseMotion(int x, int y, OpenGLState *state);

/*
* Sets a call to be performed after a sequence of view restoration is made.
* Whenever we exit some states, i.e.: QueryBar, we restore the view and keyboard
* mapping, if the transformation is intended to continue after the restoration step
* than you can use this function to be called again once the state is restored and
* make your final changes.
*/
void AppSetDelayedCall(std::function<void(void)> fn);

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
void AppCommandSplitHorizontal();
void AppCommandSplitVertical();
void AppCommandKillView();
void AppCommandKillBuffer();
void AppCommandCut();
void AppCommandJumpNesting();
void AppCommandSwitchTheme();
void AppCommandSwitchBuffer();
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
void AppCommandGitDiffCurrent();
void AppCommandGitStatus();
void AppCommandGitOpenRoot(char *path);
void AppCommandRemovePreviousToken();
void AppCommandQueryBarSearch();
void AppCommandQueryBarRevSearch();
void AppCommandQueryBarGotoLine();
void AppCommandIndentRegion(BufferView *view, vec2ui start, vec2ui end);
void AppCommandRemoveTextBlock(BufferView *bufferView, vec2ui start, vec2ui end);
void AppCommandQueryBarSearchAndReplace();
void AppCommandQueryBarInteractiveCommand();
void AppQueryBarSearchJumpToResult(QueryBar *bar, View *view);
void AppCommandOpenFileWithViewType(ViewType type);

/* Base commands for the query bar */
void AppCommandQueryBarNext();
void AppCommandQueryBarPrevious();
void AppCommandQueryBarRemoveAll();

#endif //APP_H
