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
typedef struct{
    LineBuffer lineBuffer;
    Tokenizer tokenizer;
    DoubleCursor cursor;
    vec2ui visibleRect;
    uint currentMaxRange;
    Float height;
    Float lineHeight;
    AnimationProps transitionAnim;
}BufferView;

//TODO: Implement interpolation, we could either go matrix interpolation
//      using quaternions or, simpler, detect changes and perform a interpolation
//      only on the y-axis. This could be the basis for a scroll scheme.
//      Currently we only move the line, this works in a vim-style where no half
//      line is shown, however being able to animate might give us powerfull tools.

Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity);

Float InterpolateValueLinear(Float currentInterval, Float durationInterval,
                             Float initialValue, Float finalValue);


int  BufferView_IsAnimating(BufferView *view);
Transition BufferView_GetTransitionType(BufferView *view);
void BufferView_InitalizeFromText(BufferView *view, char *content, uint size);
void BufferView_SetView(BufferView *view, Float lineHeight, Float height);
void BufferView_CursorTo(BufferView *view, uint lineNo);



void BufferView_ScrollViewRange(BufferView *view, uint lineCount, int is_up);
void BufferView_FitCursorToRange(BufferView *view, vec2ui range);

int BufferView_ComputeTextLine(BufferView *view, Float screenY);
void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col);

vec2ui BufferView_GetViewRange(BufferView *view);
vec2ui BufferView_GetCursorPosition(BufferView *view);

void BufferView_StartScrollViewTransition(BufferView *view, int lineDiffs, Float duration);
int  BufferView_GetScrollViewTransition(BufferView *view, Float dt, vec2ui *rRange,
                                        vec2ui *cursorAt, Transform *transform);

void BufferView_StartCursorTransition(BufferView *view, uint lineNo, Float duration);
int  BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                    vec2ui *cursorAt, Transform *transform);
#endif //BUFFERVIEW_H
