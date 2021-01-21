/* date = January 19th 2021 5:23 pm */

#ifndef BUFFERVIEW_H
#define BUFFERVIEW_H

#include <geometry.h>
#include <buffers.h>

typedef struct{
    vec2ui textPosition;
    vec2ui ghostPosition;
}Cursor;

typedef struct{
    LineBuffer lineBuffer;
    Tokenizer tokenizer;
    Cursor cursor;
    vec2ui visibleRect;
    Float height;
    Float lineHeight;
}BufferView;

//TODO: Implement interpolation, we could either go matrix interpolation
//      using quaternions or, simpler, detect changes and perform a interpolation
//      only on the y-axis. This could be the basis for a scroll scheme.

void BufferView_InitalizeFromText(BufferView *view, char *content, uint size);
void BufferView_SetView(BufferView *view, Float lineHeight, Float height);
void BufferView_CursorTo(BufferView *view, uint lineNo);

vec2ui BufferView_GetViewRange(BufferView *view);
vec2ui BufferView_GetCursorPosition(BufferView *view);
#endif //BUFFERVIEW_H
