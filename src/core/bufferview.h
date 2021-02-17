/* date = January 19th 2021 5:23 pm */

#ifndef BUFFERVIEW_H
#define BUFFERVIEW_H

#include <geometry.h>
#include <buffers.h>
#include <transform.h>

typedef enum{
    TransitionNone,
    TransitionCursor,
    TransitionScroll,
    TransitionNumbers,
}Transition;

typedef enum{
    DescriptionTop,
    DescriptionBottom
}DescriptionLocation;

typedef struct{
    Transition transition;
    int isAnimating;
    Float passedTime;
    Float duration;
    Float velocity;
    Float runningPosition;
    uint startLine;
    uint endLine; // this is in screen space, i.e.: 1 <= endLine <= N
    int is_down;
}AnimationProps;

typedef struct{
    vec2ui position;
    TokenId id;
    int valid;
    int comp;
    int depth;
}NestPoint;

/*
* TODO: Since this is only one of the views of a file we might
*       need to keep a pointer here instead of a copy so that
*       we can share data and better perform stuff (LineBuffer).
*       Also it might not be a good idea to make the BufferView
*       being responsable to read and parse a file (Tokenizer),
*       It might be better to simply allow the BufferView to work
*       on a already processed LineBuffer, and than every time 
*       a screen is divided we create a BufferView and simply
*       change its LineBuffer when files are swapped.
*/
struct BufferView{
    LineBuffer *lineBuffer;
    Tokenizer *tokenizer;
    DoubleCursor cursor;
    vec2ui visibleRect;
    uint currentMaxRange;
    Float lineHeight;
    AnimationProps transitionAnim;
    Geometry geometry;
    DescriptionLocation descLocation;
    
    /*
* Rendering properties of the BufferView, defined by the renderer
*/
    Float lineOffset;
    int isActive;
    int renderLineNbs;
    int activeNestPoint; // to allow jumping, -1 = none, 0 = start, 1 = end
    NestPoint startNest[64];
    NestPoint endNest[64];
    int startNestCount;
    int endNestCount;
};

Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity);

Float InterpolateValueLinear(Float currentInterval, Float durationInterval,
                             Float initialValue, Float finalValue);

/*
* Checks if a point 'p' is inside 'geometry'.
*/
int Geometry_IsPointInside(Geometry *geometry, vec2ui p);

/*
* Initializes a bufferview to represent the contents of the given linebuffer. It also
* receives the tokenizer responsible for tokenization of the linebuffer to be used
* for updates.
*/
void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer, Tokenizer *tokenizer);

/*
* Sets the geometry of a bufferview.
*/
void BufferView_SetGeometry(BufferView *view, Geometry geometry, Float lineHeight);

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

void BufferView_ScrollViewRange(BufferView *view, uint lineCount, int is_up);
void BufferView_FitCursorToRange(BufferView *view, vec2ui range);

int BufferView_ComputeTextLine(BufferView *view, Float screenY);
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
void BufferView_StartCursorTransition(BufferView *view, uint lineNo, Float duration);

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
Float BufferView_GetDescription(BufferView *view, char *content, uint size);

/*
* Allows the bufferview 'view' to synchronize and avoid out of bounds
* errors in case 'source' changed properties of the 'view'.
*/
void BufferView_SynchronizeWith(BufferView *view, BufferView *source);

void BufferView_Test(BufferView *view);
#endif //BUFFERVIEW_H
