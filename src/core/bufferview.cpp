#include <bufferview.h>

void BufferView_InitalizeFromText(BufferView *view, char *content, uint size){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    view->lineBuffer = (LineBuffer)LINE_BUFFER_INITIALIZER;
    view->tokenizer  = (Tokenizer) TOKENIZER_INITIALIZER;
    
    Lex_BuildTokenizer(&view->tokenizer);
    LineBuffer_Init(&view->lineBuffer, &view->tokenizer, content, size);
    view->cursor.textPosition  = vec2ui(0, 0);
    view->cursor.ghostPosition = vec2ui(0, 0);
    view->visibleRect = vec2ui(0, 0);
    view->lineHeight = 0;
    view->height = 0;
}

void BufferView_SetView(BufferView *view, Float lineHeight, Float height){
    AssertA(view != nullptr && !IsZero(lineHeight), "Invalid inputs");
    //TODO: Need to recompute the correct position as cursor might be hidden now
    uint yRange = (uint)floor(height / lineHeight);
    view->height = height;
    view->lineHeight = lineHeight;
    view->visibleRect.y = view->visibleRect.x + yRange;
    BufferView_CursorTo(view, view->cursor.textPosition.x); //TODO: Does this fix?
}

vec2ui BufferView_GetViewRange(BufferView *view){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    vec2ui rect = view->visibleRect;
    //printf("Line Range: [%u - %u] Cursor: %u\n", rect.x, rect.y, view->cursor.textPosition.x);
    return rect;
}

void BufferView_CursorTo(BufferView *view, uint lineNo){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    int gap = 2;
    vec2ui visibleRect = view->visibleRect;
    uint lineRangeSize = visibleRect.y - visibleRect.x;
    if(!(lineNo < visibleRect.y-gap && lineNo > visibleRect.x+gap)){
        // Outside screen
        if(lineNo <= visibleRect.x + gap && visibleRect.x != 0){ // going up
            int iLine = (int)lineNo;
            iLine = Max(0, iLine - gap - 1);
            visibleRect.x = (uint)iLine;
            visibleRect.y = visibleRect.x + lineRangeSize;
        }else if(lineNo >= visibleRect.y - gap){ // going down
            visibleRect.y = Min(lineNo + gap + 1, view->lineBuffer.lineCount);
            visibleRect.x = visibleRect.y - lineRangeSize;
        }
    }
    
    view->visibleRect = visibleRect;
    if(lineNo < view->lineBuffer.lineCount){
        view->cursor.textPosition.x = lineNo;
    }
}

vec2ui BufferView_GetCursorPosition(BufferView *view){
    return view->cursor.textPosition;
}