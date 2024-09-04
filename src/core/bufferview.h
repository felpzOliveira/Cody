/* date = January 19th 2021 5:23 pm */

#ifndef BUFFERVIEW_H
#define BUFFERVIEW_H

#include <geometry.h>
#include <buffers.h>
#include <transform.h>
#include <scroll_controller.h>
#include <functional>
#include <optional>
#include <pdfview.h>
#include <image_renderer.h>

#define LHINT_STATE_VALID      1
#define LHINT_STATE_UNKNOW     2
#define LHINT_STATE_INTERRUPT  3

typedef enum{
    DescriptionTop,
    DescriptionBottom
}DescriptionLocation;

typedef enum{
    CodeView,
    GitStatusView,
    GitDiffView,
    DbgView,
    ImageView,
    EmptyView,
    NoneView,
}ViewType;

typedef struct{
    vec2ui position;
    TokenId id;
    int valid;
    int comp;
    int depth;
}NestPoint;

typedef struct{
    Float currX;
    Transform horizontal;
}Scroll;

struct LineHints{
    uint line;
    int state; // this might not be the best name
};

struct BufferView{
    LineBuffer *lineBuffer;
    PdfViewState *pdfView;
    ImageRenderer imgRenderer;
    ImageRenderer scrollImgRenderer;
    VScroll sController;
    Geometry geometry;
    ViewType activeType;
    /*
    * Rendering properties of the BufferView, defined by the renderer
    */
    Scroll scroll;
    Float lineOffset;
    int isActive;
    int renderLineNbs;
    int activeNestPoint; // to allow jumping, -1 = none, 0 = start, 1 = end
    NestPoint startNest[64];
    NestPoint endNest[64];
    int startNestCount;
    int endNestCount;
    int is_visible;
    int is_mouse_pressed;
    int is_range_selected;

    // test this
    std::optional<LineHints> highlightLine;

    int is_transitioning;
};

struct BufferViewFileLocation{
    vec2ui textPosition;
    vec2ui ghostPosition;
    vec2ui range;
    LineBuffer *lineBuffer;
};

inline const char *ViewTypeString(ViewType type){
#define STR_CASE(x) case x : return #x
    switch(type){
        STR_CASE(CodeView);
        STR_CASE(GitDiffView);
        STR_CASE(GitStatusView);
        STR_CASE(DbgView);
        STR_CASE(EmptyView);
        default: return "(none)";
    }
#undef STR_CASE
}

/*
* Initializes a bufferview to represent the contents of the given linebuffer.
*/
void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer, ViewType type);

/*
* Makes a bufferview uses a linebuffer for file reference.
*/
void BufferView_SwapBuffer(BufferView *view, LineBuffer *lineBuffer, ViewType type);

/*
* Sets the bufferview pdfview.
*/
void BufferView_SetPdfView(BufferView *view, PdfViewState *pdfView);

/*
* Gets the bufferview pdfview.
*/
PdfViewState *BufferView_GetPdfView(BufferView *view);

/*
* Gets the image renderer.
*/
ImageRenderer *BufferView_GetImageRenderer(BufferView *view);

/*
* Resets/inits the image renderer.
*/
void BufferView_ResetImageRenderer(BufferView *view, PdfRenderPages pages);

/*
* Retrieves the view type of the given bufferview.
*/
ViewType BufferView_GetViewType(BufferView *view);

/*
* Retrieves the maximum line count given bufferview can display simultaneously,
* i.e.: the size of one page.
*/
uint BufferView_GetMaximumLineView(BufferView *view);

/*
* Sets the view type of the given bufferview. Usually you want to set this value during
* initialization or buffer swap as it is bounded by the linebuffer. However some
* operations i.e: git diff, work on the contents of the linebuffer and do not have
* a dedicated linebuffer, so you can use this to guide the state of the linebuffer.
*/
void BufferView_SetViewType(BufferView *view, ViewType type);

/*
* Sets the geometry of a bufferview.
*/
void BufferView_SetGeometry(BufferView *view, Geometry geometry, Float lineHeight);

/*
* Gets the geometry of a bufferview.
*/
void BufferView_GetGeometry(BufferView *view, Geometry *geometry);

/*
* Queries the bufferview if it is _at_this_moment_ in an animation state.
*/
int BufferView_IsAnimating(BufferView *view);

/*
* Queries the bufferview for the current transition that is active in case the bufferview
* is in an animation state.
*/
Transition BufferView_GetTransitionType(BufferView *view);

/*
* Moves the cursor to 'lineNo' and recompute visible range.
*/
void BufferView_CursorTo(BufferView *view, uint lineNo);

/*
* Forces the ghost cursor to go to the current cursor position.
*/
void BufferView_GhostCursorFollow(BufferView *view);

/*
* Queries the bufferview for the next token to jump when performing line jumping
* routines. Returns -1 in case no token is available or the id of the token.
*/
int BufferView_LocateNextCursorToken(BufferView *view, Token **token);

/*
* Queries the bufferview for the previous token to jump when performing line jumping
* routines. Returns -1 in case no token is available or the id of the token.
*/
int BufferView_LocatePreviousCursorToken(BufferView *view, Token **token);

/*
* Forces the cursor of the given bufferview to update its position based on
* a specific range of visible lines.
*/
void BufferView_FitCursorToRange(BufferView *view, vec2ui range);

/*
* Computes the line number corresponding to a screen position, returns -1
* if no line is available.
*/
int BufferView_ComputeTextLine(BufferView *view, Float screenY, DescriptionLocation desc);

/*
* Moves the cursor to a specific position in a given bufferview.
*/
void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col);

/*
* Returns the pointer to the current cursor.
*/
void BufferView_GetCursor(BufferView *view, DoubleCursor **cursor);

/*
* Get the Buffer located inside the LineBuffer at a specific line.
*/
Buffer *BufferView_GetBufferAt(BufferView *view, uint lineNo);

/*
* Get the LineBuffer being used by this BufferView.
*/
LineBuffer *BufferView_GetLineBuffer(BufferView *view);

/*
* Queries the bufferview for the current range of lines that need to be rendered,
* according to the current cursor position and the geometry of the bufferview.
*/
vec2ui BufferView_GetViewRange(BufferView *view);

/*
* Returns the current cursor position.
*/
vec2ui BufferView_GetCursorPosition(BufferView *view);

/*
* Returns the current position of the ghost cursor.
*/
vec2ui BufferView_GetGhostCursorPosition(BufferView *view);

/*
* Starts a scroll view transition.
*/
void BufferView_StartScrollViewTransition(BufferView *view, int lineDiffs, Float duration);

/*
* Queries the bufferview for the state of the started scroll transition after 'dt'.
* Returns the estimated cursor position in 'cursorAt', the range view in 'rRange' 
* and the 0-1 transform for scrolling.
*/
int BufferView_GetScrollViewTransition(BufferView *view, Float dt, vec2ui *rRange,
                                       vec2ui *cursorAt, Transform *transform);

/*
* Gets  the range selected by the ghost cursor and the cursor in 'start' and 'end'.
* It also returns 0 in case there is no range, i.e.: both are in the same position
* or 1 in case the range > 0.
*/
uint BufferView_GetCursorSelectionRange(BufferView *view, vec2ui *start, vec2ui *end);

/*
* Starts a 'animation' on line numbering, not really animation but we use as such
* to prevent graphics to enter a _wait_for_event_ state.
*/
void BufferView_StartNumbersShowTransition(BufferView *view, Float duration);

/*
* Queries the bufferview for the state of the started number transition.
*/
int BufferView_GetNumbersShowTransition(BufferView *view, Float dt);

/*
* Starts a animation on the cursor.
*/
void BufferView_StartCursorTransition(BufferView *view, uint lineNo,
                                      uint col, Float duration);

/*
* Queries the bufferview for the state of a started cursor animation after the time
* 'dt' is executed over it. Returns the estimated cursor position in 'cursorAt',
* the range view in 'rRange' and the 0-1 transform for scrolling.
*/
int BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                   vec2ui *cursorAt, Transform *transform);

/*
* Queries the bufferview if its cursor nest is valid, i.e.: needs rendering.
*/
int BufferView_CursorNestIsValid(BufferView *view);

/*
* Allows the bufferview to update its cursor nesting if it needs to do so.
*/
void BufferView_UpdateCursorNesting(BufferView *view);

/*
* Computes the start and end of the nesting containing the cursor.
*/
void BufferView_UpdateCursorNesting(BufferView *view);

/*
* Adjusts the ghost cursor if it is out of bounds.
*/
void BufferView_AdjustGhostCursorIfOut(BufferView *view);

/*
* Toogles the rendering of line numbers in the BufferView 'view'.
*/
void BufferView_ToogleLineNumbers(BufferView *view);

/*
* Returns the amount of lines present the LineBuffer binded to a BufferView 'view'.
*/
uint BufferView_GetLineCount(BufferView *view);

/*
* Returns a description string for printing to the screen and the current
* cursor position, in relation to the file head, .i.e: percetage, 0 means
* cursor at the head while 1 is at the end of the file.
*/
Float BufferView_GetDescription(BufferView *view, char *content, uint size,
                                char *endDesc, uint endSize);

/*
* Allows the bufferview 'view' to synchronize and avoid out of bounds
* errors in case the underlying linebuffer changed.
*/
void BufferView_Synchronize(BufferView *view);

/*
* Marks the linebuffer present in the bufferview 'view' as dirty, i.e.: needing save.
*/
void BufferView_Dirty(BufferView *view);

/*
* Checks if a given bufferview is currently being rendered.
*/
int BufferView_IsVisible(BufferView *view);

/*
* Gets the highlight line value, note that this is optinal.
*/
std::optional<LineHints> BufferView_GetLineHighlight(BufferView *view);

/*
* Sets the state of the mouse pressed event.
*/
void BufferView_SetMousePressed(BufferView *view, int pressed);

/*
* Gets the state of the mouse pressed event.
*/
int BufferView_GetMousePressed(BufferView *view);

/*
* Sets the state of the mouse range visible rect.
*/
void BufferView_SetRangeVisible(BufferView *view, int visible);

/*
* Gets the state of the mouse range visible rect.
*/
int BufferView_GetRangeVisible(BufferView *view);

/*
* Sets the highlight line value.
*/
void BufferView_SetHighlightLine(BufferView *view, LineHints hint);

/*
* Clear the highlight line value.
*/
void BufferView_ClearHighlightLine(BufferView *view);

/*
* Sets a callback to be triggered whenever scroll animation finishes.
*/
void BufferView_SetAnimationCallback(BufferView *view, std::function<void()> fn);

/*
* Find the position of a target token given by 'id' that have a counterpart 'cid',
* starting from position 'start'. Returns true in case it is found false otherwise,
* 'end' holds the (line, tid) where token is located.
*/
bool BufferView_FindFirstForward(BufferView *view, TokenId id, TokenId cid,
                                 vec2ui start, vec2ui &end);

/*
* Register the position of the bufferview for a linebuffer.
*/
void BufferViewFileLocation_Register(BufferView *view);

/*
* Restore the position of a bufferview for a linebuffer.
*/
void BufferViewFileLocation_Restore(BufferView *view);

/*
* Removes a bufferview from the global tracked views.
*/
void BufferViewLocation_RemoveView(BufferView *view);

/*
* Removes a linebuffer from all tracked views positions.
*/
void BufferViewLocation_RemoveLineBuffer(LineBuffer *lineBuffer);

#endif //BUFFERVIEW_H
