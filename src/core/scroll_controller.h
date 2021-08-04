/* date = February 25th 2021 6:00 pm */

#ifndef SCROLL_CONTROLLER_H
#define SCROLL_CONTROLLER_H
#include <geometry.h>
#include <buffers.h>
#include <transform.h>

typedef enum{
    TransitionNone,
    TransitionCursor,
    TransitionScroll,
    TransitionNumbers,
}Transition;

typedef struct{
    Transition transition;
    int isAnimating;
    Float passedTime;
    Float duration;
    Float velocity;
    Float runningPosition;
    uint startLine;
    uint endLine; // this is in screen space, i.e.: 1 <= endLine <= N
    uint endCol;
    int is_down;
}AnimationProps;

struct VScroll{
    DoubleCursor cursor;
    vec2ui visibleRect;
    uint currentMaxRange;
    Float lineHeight;
    AnimationProps transitionAnim;
    std::function<void()> onAnimationFinished;
};

void VScroll_Init(VScroll *ss);

void VScroll_GhostCursorFollow(VScroll *ss);
void VScroll_AdjustGhostIfNeeded(VScroll *ss, LineBuffer *lineBuffer);
void VScroll_SyncWith(VScroll *ss, VScroll *source);
uint VScroll_GetCursorSelection(VScroll *ss, vec2ui *start, vec2ui *end);
vec2ui VScroll_GetCursorPosition(VScroll *ss);

void VScroll_StartScrollViewTransition(VScroll *ss, int lineDiffs,
                                       Float duration, LineBuffer *lineBuffer);

int VScroll_GetScrollViewTransition(VScroll *ss, Float dt, vec2ui *rRange,
                                    vec2ui *cursorAt, Transform *transform,
                                    LineBuffer *lineBuffer);

void VScroll_StartCursorTransition(VScroll *ss, uint lineNo, uint col,
                                   Float duration, LineBuffer *lineBuffer);

int VScroll_GetCursorTransition(VScroll *ss, Float dt, vec2ui *rRange,
                                vec2ui *cursorAt, Transform *transform,
                                LineBuffer *lineBuffer);

void VScroll_CursorToPosition(VScroll *ss, uint lineNo, uint col, LineBuffer *lineBuffer);

void VScroll_CursorTo(VScroll *ss, uint lineNo, LineBuffer *lineBuffer);

void VScroll_FitCursorToRange(VScroll *ss, vec2ui range, LineBuffer *lineBuffer);

void VScroll_SetAnimationFinishCallback(VScroll *ss, std::function<void()> fn);

#endif //SCROLL_CONTROLLER_H
