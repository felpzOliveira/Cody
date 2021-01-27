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
    view->transitionAnim.isAnimating = 0;
    view->transitionAnim.duration = 0.0f;
    view->transitionAnim.endLine = 0;
    view->transitionAnim.velocity = 0;
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
    return vec2ui(view->visibleRect.x, Min(view->visibleRect.y,
                                           view->lineBuffer.lineCount));
}

void BufferView_ScrollViewRange(BufferView *view, uint lineCount, int is_up){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    vec2ui rect = BufferView_GetViewRange(view);
    uint totalLines = view->lineBuffer.lineCount;
    uint range = view->visibleRect.y - view->visibleRect.x;
    
    if(is_up){
        rect.x = rect.x > lineCount ? rect.x - lineCount : 0;
    }else{
        rect.x = rect.x + lineCount < totalLines ? rect.x + lineCount : totalLines-1;
    }
    
    rect.y = rect.x + range;
    view->visibleRect = rect;
    BufferView_FitCursorToRange(view, rect);
}

void BufferView_FitCursorToRange(BufferView *view, vec2ui range){
    Buffer *buffer = nullptr;
    vec2ui cCoord = view->cursor.textPosition;
    int X = (int)range.x;
    int Y = (int)range.y;
    int lineNo = (int)cCoord.x;
    int gap = 2;
    if(!(lineNo < Y - gap && lineNo > X + gap)){
        if(lineNo <= X + gap && X != 0){
            lineNo = Min(view->lineBuffer.lineCount, X + gap + 1);
        }else if(lineNo >= Y - gap){
            lineNo = Max(Y - gap - 1, 0);
        }
    }
    
    buffer = &view->lineBuffer.lines[lineNo];
    
    view->cursor.textPosition.x = lineNo;
    if(buffer->tokenCount > 0){
        uint lastP = buffer->tokens[buffer->tokenCount-1].position;
        uint lastX = lastP + buffer->tokens[buffer->tokenCount-1].size;
        
        if(view->cursor.textPosition.y < buffer->tokens[0].position){
            view->cursor.textPosition.y = buffer->tokens[0].position;
        }else if(view->cursor.textPosition.y > lastX){
            view->cursor.textPosition.y = lastX;
        }
    }else{
        view->cursor.textPosition.y = 0;
    }
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

int BufferView_ComputeTextLine(BufferView *view, Float screenY){
    Buffer *buffer = nullptr;
    Float lStart = view->visibleRect.x;
    Float fline = lStart + floor(screenY / view->lineHeight);
    if((uint)fline < view->lineBuffer.lineCount-1){
        return (int)fline;
    }
    
    return -1;
}

void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col){
    Buffer *buffer = LineBuffer_GetBufferAt(&view->lineBuffer, (uint)lineNo);
    if(buffer){
        col = Clamp(col, 0, buffer->size-1);
        BufferView_CursorTo(view, (uint)lineNo);
        view->cursor.textPosition.y = col;
    }
}

void BufferView_StartCursorTransition(BufferView *view, uint lineNo, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    lineNo = Clamp(lineNo, 0, view->lineBuffer.lineCount-1);
    if(IsZero(duration) || duration < 0){ // if no interval is given simply move
        BufferView_CursorTo(view, lineNo);
    }else if(view->cursor.textPosition.x != lineNo){ // actual animation
        AssertA(view->transitionAnim.isAnimating == 0, "Only one animation at a time");
        view->transitionAnim.isAnimating = 1;
        view->transitionAnim.passedTime = 0;
        view->transitionAnim.duration = duration;
        view->transitionAnim.endLine = lineNo;
        view->transitionAnim.velocity = 0;
        view->transitionAnim.runningPosition = -1;
        view->transitionAnim.is_down = view->cursor.textPosition.x < lineNo;
    }else{
        //AssertA(0, "Invalid transition requested");
    }
}

Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity)
{
    Float t  = dt / remaining;
    Float u  = (1.0f - t);
    Float c0 = 1.0f * u * u * u;
    Float c1 = 3.0f * u * u * t;
    Float c2 = 3.0f * u * t * t;
    Float c3 = 1.0f * t * t * t;
    
    Float dc0 = -3.0f * u * u;
    Float dc1 = -6.0f * u * t + 3.0f * u * u;
    Float dc2 =  6.0f * u * t - 3.0f * t * t;
    Float dc3 =  3.0f * t * t;
    
    Float b0 = *initialValue;
    Float b1 = *initialValue + (remaining / 3.0f) * (*velocity);
    Float b2 = finalValue;
    Float b3 = b2;
    
    *initialValue = c0 * b0 + c1 * b1 + c2 * b2 + c3 * b3;
    *velocity = (dc0 * b0 + dc1 * b1 + dc2 * b2 + dc3 * b3) * (1.0f / remaining);
    
    return *initialValue;
}

Float InterpolateValueLinear(Float currentInterval, Float durationInterval,
                             Float initialValue, Float finalValue)
{
    Float intervalFract = currentInterval / durationInterval;
    return intervalFract * (finalValue - initialValue) + initialValue;
}

int BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                   vec2ui *cursorAt, Transform *transform)
{
    int gap = 2;
    AnimationProps *anim = &view->transitionAnim;
    
    if(anim->runningPosition < 0){
        anim->runningPosition = (Float)view->cursor.textPosition.x;
    }
    
    anim->passedTime += Max(0, dt);
    if(IsZero(anim->passedTime - anim->duration) ||
       anim->passedTime > anim->duration)
    { // This transition is done
        goto __set_end_transition;
    }else{
        Buffer *buffer = nullptr;
        vec2ui rect = view->visibleRect;
        uint range = rect.y - rect.x;
#if 0
        Float lak = InterpolateValueLinear(anim->passedTime, anim->duration,
                                           (Float)view->cursor.textPosition.x,
                                           (Float)anim->endLine);
#else
        Float remaining = anim->duration - anim->passedTime;
        Float lak = InterpolateValueCubic(dt, remaining, &anim->runningPosition,
                                          (Float)anim->endLine, &anim->velocity);
#endif
        cursorAt->x = (uint)Max(0, round(lak));
        buffer = LineBuffer_GetBufferAt(&view->lineBuffer, cursorAt->x);
        if(buffer){
            uint initialp = 0;
            if(buffer->tokenCount > 0) initialp = buffer->tokens[0].position;
            cursorAt->y = Clamp(view->cursor.textPosition.y,initialp, buffer->size-1);
        }
        
        if(anim->is_down){
            if(!(cursorAt->x < rect.y - gap)){
                rRange->y = Min(cursorAt->x + gap + 1, view->lineBuffer.lineCount-1);
                int minV = (int)rRange->y - (int)range;
                
                rRange->x = (uint)Max(0, minV);
                *transform = Translate(0, Fract(lak), 0);
            }else{
                rRange->x = rect.x;
                rRange->y = rect.y;
                *transform = Transform();
            }
            
            if(cursorAt->x >= anim->endLine){ // incorrect interpolation, end transition
                goto __set_end_transition;
            }
        }else{
            if(!(cursorAt->x > rect.x + gap)){
                rRange->x = Max(0, cursorAt->x - gap - 1);
                rRange->y = Min(rRange->x + range + 1, view->lineBuffer.lineCount-1);
                *transform = Translate(0, -Fract(lak), 0);
            }else{
                rRange->x = rect.x;
                rRange->y = rect.y;
                *transform = Transform();
            }
            
            if(cursorAt->x <= anim->endLine){ // incorrect interpolation, end transition
                goto __set_end_transition;
            }
        }
    }
    
    return 0;
    __set_end_transition:
    BufferView_CursorToPosition(view, anim->endLine, 0);
    rRange->x = view->visibleRect.x;
    rRange->y = view->visibleRect.y;
    cursorAt->x = view->cursor.textPosition.x;
    cursorAt->y = view->cursor.textPosition.y;
    *transform = Transform();
    anim->isAnimating = 0;
    return 1;
}

vec2ui BufferView_GetCursorPosition(BufferView *view){
    return view->cursor.textPosition;
}

int BufferView_IsAnimating(BufferView *view){
    return view->transitionAnim.isAnimating;
}