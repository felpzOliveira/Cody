/* date = January 19th 2021 5:23 pm */

#ifndef BUFFERVIEW_H
#define BUFFERVIEW_H

#include <geometry.h>
#include <buffers.h>

typedef struct{
    vec2ui textPosition;
    vec2ui ghostPosition;
}DoubleCursor;

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
    Float height;
    Float lineHeight;
}BufferView;

//TODO: Implement interpolation, we could either go matrix interpolation
//      using quaternions or, simpler, detect changes and perform a interpolation
//      only on the y-axis. This could be the basis for a scroll scheme.
//      Currently we only move the line, this works in a vim-style where no half
//      line is shown, however being able to animate might give us powerfull tools.

void BufferView_InitalizeFromText(BufferView *view, char *content, uint size);
void BufferView_SetView(BufferView *view, Float lineHeight, Float height);
void BufferView_CursorTo(BufferView *view, uint lineNo);

vec2ui BufferView_GetViewRange(BufferView *view);
vec2ui BufferView_GetCursorPosition(BufferView *view);
#endif //BUFFERVIEW_H
