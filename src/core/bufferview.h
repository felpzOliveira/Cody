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
}Transition;

typedef struct{
    vec2ui textPosition;
    vec2ui ghostPosition;
}DoubleCursor;

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
    Float height;
    Float lineHeight;
    AnimationProps transitionAnim;
    Geometry geometry;
    
    /*
* Rendering properties of the BufferView, defined by the renderer
*/
    Float lineOffset;
    int isActive;
    int renderLineNbs;
};

Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity);

Float InterpolateValueLinear(Float currentInterval, Float durationInterval,
                             Float initialValue, Float finalValue);

int Geometry_IsPointInside(Geometry *geometry, vec2ui p);

void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer, Tokenizer *tokenizer);

void BufferView_SetGeometry(BufferView *view, Geometry geometry,
                            Float lineHeight);

int  BufferView_IsAnimating(BufferView *view);
Transition BufferView_GetTransitionType(BufferView *view);
void BufferView_CursorTo(BufferView *view, uint lineNo);
void BufferView_GhostCursorFollow(BufferView *view);

int BufferView_LocateNextCursorToken(BufferView *view, Token **token);
int BufferView_LocatePreviousCursorToken(BufferView *view, Buffer *buffer, Token **token);

void BufferView_ScrollViewRange(BufferView *view, uint lineCount, int is_up);
void BufferView_FitCursorToRange(BufferView *view, vec2ui range);

int BufferView_ComputeTextLine(BufferView *view, Float screenY);
void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col);

Buffer *BufferView_GetBufferAt(BufferView *view, uint lineNo);
LineBuffer *BufferView_GetLineBuffer(BufferView *view);

vec2ui BufferView_GetViewRange(BufferView *view);
vec2ui BufferView_GetCursorPosition(BufferView *view);
vec2ui BufferView_GetGhostCursorPosition(BufferView *view);

void BufferView_StartScrollViewTransition(BufferView *view, int lineDiffs, Float duration);
int  BufferView_GetScrollViewTransition(BufferView *view, Float dt, vec2ui *rRange,
                                        vec2ui *cursorAt, Transform *transform);

void BufferView_StartCursorTransition(BufferView *view, uint lineNo, Float duration);
int  BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                    vec2ui *cursorAt, Transform *transform);

void BufferView_ToogleLineNumbers(BufferView *view);

uint BufferView_GetLineCount(BufferView *view);
#endif //BUFFERVIEW_H
