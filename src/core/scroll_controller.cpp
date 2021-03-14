#include <scroll_controller.h>

int Animation_Finished(AnimationProps *anim);
Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity);

Float InterpolateValueLinear(Float currentInterval, Float durationInterval,
                             Float initialValue, Float finalValue);

inline void VScroll_UpdateRelativeDistance(VScroll *ss, uint line, uint maxn){
    ss->cursor.relativeDistance.x = line;
    ss->cursor.relativeDistance.y = maxn > line ? maxn - line : 0;
}

inline int VScroll_IsCursorVisible(VScroll *ss, vec2ui range){
    return (range.x <= ss->cursor.textPosition.x &&
            range.y >= ss->cursor.textPosition.x) ? 1 : 0;
}

void VScroll_GhostCursorFollow(VScroll *ss){
    ss->cursor.ghostPosition = ss->cursor.textPosition;
}

void VScroll_SyncWith(VScroll *ss, VScroll *source){
    ss->cursor = source->cursor;
}

vec2ui VScroll_GetCursorPosition(VScroll *ss){
    return ss->cursor.textPosition;
}

void VScroll_Init(VScroll *ss){
    ss->cursor.textPosition  = vec2ui(0, 0);
    ss->cursor.ghostPosition = vec2ui(0, 0);
    ss->cursor.relativeDistance = vec2ui(0, 0);
    ss->cursor.is_dirty = 1;
    ss->cursor.nestStart = vec2ui(0, 0);
    ss->cursor.nestEnd = vec2ui(0, 0);
    ss->cursor.nestValid = 0;
    ss->visibleRect = vec2ui(0, 0);
    ss->lineHeight = 0;
    ss->transitionAnim.transition = TransitionNone;
    ss->transitionAnim.isAnimating = 0;
    ss->transitionAnim.duration = 0.0f;
    ss->transitionAnim.endLine = 0;
    ss->transitionAnim.velocity = 0;
}

uint VScroll_GetCursorSelection(VScroll *ss, vec2ui *start, vec2ui *end){
    vec2ui s = ss->cursor.textPosition;
    vec2ui e = ss->cursor.ghostPosition;
    
    if(s.x > e.x){ // ghost cursor is behind
        s = ss->cursor.ghostPosition;
        e = ss->cursor.textPosition;
    }else if(s.x == e.x){ // same line
        if(s.y == e.y){ // range is 0
            return 0;
        }
        if(s.y > e.y){ // cursor is foward in line
            s = ss->cursor.ghostPosition;
            e = ss->cursor.textPosition;
        }
    }
    
    *start = s;
    *end = e;
    return 1;
}

void VScroll_AdjustGhostIfNeeded(VScroll *ss, LineBuffer *lineBuffer){
    Buffer *b = LineBuffer_GetBufferAt(lineBuffer, ss->cursor.ghostPosition.x);
    if(b == nullptr){ // in case there was a delete this will be nullptr
        VScroll_GhostCursorFollow(ss);
    }else if(!(ss->cursor.ghostPosition.y < b->count)){
        VScroll_GhostCursorFollow(ss);
    }
}


void VScroll_FitCursorToRange(VScroll *ss, vec2ui range, LineBuffer *lineBuffer){
    Buffer *buffer = nullptr;
    vec2ui cCoord = ss->cursor.textPosition;
    int X = (int)range.x;
    int Y = (int)range.y;
    int lineNo = (int)cCoord.x;
    int gap = 2;
    if(!(lineNo < Y - gap && lineNo > X + gap)){
        if(lineNo < X + gap){
            lineNo = Min(lineBuffer->lineCount-1, X + gap);
        }else if(lineNo > Y - gap){
            lineNo = Max(Y - gap, 0);
            int range = Y - X;
            if(range < (int)ss->currentMaxRange) lineNo++;
        }
    }
    
    buffer = LineBuffer_GetBufferAt(lineBuffer, lineNo);
    
    ss->cursor.textPosition.x = lineNo;
    if(buffer->tokenCount > 0){
        uint lastP = buffer->tokens[buffer->tokenCount-1].position;
        uint lastX = lastP + buffer->tokens[buffer->tokenCount-1].size;
        
        if((int)ss->cursor.textPosition.y < buffer->tokens[0].position){
            ss->cursor.textPosition.y = buffer->tokens[0].position;
        }else if(ss->cursor.textPosition.y > lastX){
            ss->cursor.textPosition.y = lastX;
        }
    }else{
        ss->cursor.textPosition.y = 0;
    }
    
    VScroll_UpdateRelativeDistance(ss, lineNo, lineBuffer->lineCount);
    ss->cursor.is_dirty = 1;
}


void VScroll_CursorToPosition(VScroll *ss, uint lineNo, uint col, LineBuffer *lineBuffer){
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, (uint)lineNo);
    if(buffer){
        if(buffer->count > 0)
            col = Clamp(col, (uint)0, buffer->count);
        else
            col = 0;
        
        VScroll_CursorTo(ss, (uint)lineNo, lineBuffer);
        ss->cursor.textPosition.y = col;
        
        VScroll_UpdateRelativeDistance(ss, lineNo, lineBuffer->lineCount);
    }
}

void VScroll_CursorTo(VScroll *ss, uint lineNo, LineBuffer *lineBuffer){
    AssertA(ss != nullptr, "Invalid scroll controller pointer");
    int gap = 1;
    vec2ui visibleRect = ss->visibleRect;
    uint lineRangeSize = ss->currentMaxRange;

    if(!(lineNo < visibleRect.y-gap && lineNo > visibleRect.x+gap)){
        if(lineNo <= visibleRect.x + gap && visibleRect.x != 0){ // going up
            int iLine = (int)lineNo;
            iLine = Max(0, iLine - gap);
            visibleRect.x = (uint)iLine;
            visibleRect.y = Min(visibleRect.x + lineRangeSize, lineBuffer->lineCount);
        }else if(lineNo >= visibleRect.y - gap){ // going down
            uint dif = visibleRect.x + lineRangeSize - visibleRect.y;
            uint wished = Min(lineNo + gap + 1, lineBuffer->lineCount);
            visibleRect.y = wished;
            if(visibleRect.y + dif <= wished){
                visibleRect.x = wished - lineRangeSize;
            }
        }
    }

    ss->visibleRect = visibleRect;
    if(lineNo < lineBuffer->lineCount){
        ss->cursor.textPosition.x = lineNo;
        VScroll_UpdateRelativeDistance(ss, lineNo, lineBuffer->lineCount);
    }

    ss->cursor.is_dirty = 1;
}

void VScroll_CursorTo2(VScroll *ss, uint lineNo, LineBuffer *lineBuffer){
    AssertA(ss != nullptr, "Invalid scroll controller pointer");
    int gap = 1;
    vec2ui visibleRect = ss->visibleRect;
    uint lineRangeSize = ss->currentMaxRange;
    if(!(lineNo < visibleRect.y-gap && lineNo > visibleRect.x+gap)){
        // Outside screen
        if(lineNo <= visibleRect.x + gap && visibleRect.x != 0){ // going up
            int iLine = (int)lineNo;
            iLine = Max(0, iLine - gap);
            visibleRect.x = (uint)iLine;
            visibleRect.y = Min(visibleRect.x + lineRangeSize, lineBuffer->lineCount);
        }else if(lineNo >= visibleRect.y - gap){ // going down
#if 1
            uint currRange = visibleRect.y - visibleRect.x;
            if(lineNo == lineBuffer->lineCount - 1){
                if(currRange > 1){
                    visibleRect.y = lineBuffer->lineCount;
                    visibleRect.x = visibleRect.y - currRange;
                }
            }else{
                visibleRect.y = Min(lineNo + gap + 1, lineBuffer->lineCount);
                visibleRect.x = visibleRect.y - lineRangeSize;
            }
#endif
        }
    }
    
    ss->visibleRect = visibleRect;
    if(lineNo < lineBuffer->lineCount){
        ss->cursor.textPosition.x = lineNo;
        VScroll_UpdateRelativeDistance(ss, lineNo, lineBuffer->lineCount);
    }
    
    ss->cursor.is_dirty = 1;
}

void VScroll_StartScrollViewTransition(VScroll *ss, int lineDiffs,
                                       Float duration, LineBuffer *lineBuffer)
{
    AssertA(ss != nullptr, "Invalid scroll controller pointer");
    int expectedEnd = (int)ss->visibleRect.x + lineDiffs;
    if(ss->transitionAnim.isAnimating == 0){
        if(lineDiffs < 0){ // going up
            expectedEnd = Max(0, expectedEnd);
        }else{ // going down
            expectedEnd = Min(lineBuffer->lineCount - 1, expectedEnd);
        }
        
        ss->transitionAnim.isAnimating = 1;
        ss->transitionAnim.transition = TransitionScroll;
        ss->transitionAnim.passedTime = 0;
        ss->transitionAnim.duration = duration;
        ss->transitionAnim.endLine = expectedEnd;
        ss->transitionAnim.velocity = 0;
        ss->transitionAnim.startLine = ss->visibleRect.x;
        ss->transitionAnim.runningPosition = (Float)ss->visibleRect.x;
        ss->transitionAnim.is_down = lineDiffs > 0;
    }else if(ss->transitionAnim.transition == TransitionScroll){
        expectedEnd = ss->transitionAnim.endLine + lineDiffs;
        if(lineDiffs < 0){ // going up
            expectedEnd = Max(0, expectedEnd);
        }else{ // going down
            expectedEnd = Min(lineBuffer->lineCount - 1, expectedEnd);
        }
        
        int is_down = expectedEnd > (int)ss->transitionAnim.endLine;
        ss->transitionAnim.endLine = expectedEnd;
        ss->transitionAnim.is_down = is_down;
    }
}

int VScroll_GetScrollViewTransition(VScroll *ss, Float dt, vec2ui *rRange,
                                    vec2ui *cursorAt, Transform *transform,
                                    LineBuffer *lineBuffer)
{
    AnimationProps *anim = &ss->transitionAnim;
    vec2ui oldP = ss->cursor.textPosition;
    uint range = ss->currentMaxRange;
    
    AssertA(anim->transition == TransitionScroll, "Incorrect transition query");
    ss->cursor.is_dirty = 1;
    anim->passedTime += Max(0, dt);
    if(Animation_Finished(anim)){
        goto __set_end_transition;
    }else{
#if 0
        Float lak = InterpolateValueLinear(anim->passedTime, anim->duration,
                                           (Float)anim->startLine, (Float)anim->endLine);
#else
        Float remaining = anim->duration - anim->passedTime;
        Float lak = InterpolateValueCubic(dt, remaining, &anim->runningPosition,
                                          (Float)anim->endLine, &anim->velocity);
#endif
        ss->visibleRect.x = Max(0, Floor(lak));
        ss->visibleRect.y = Min(ss->visibleRect.x + range,
                                lineBuffer->lineCount);
        
        rRange->x = ss->visibleRect.x;
        rRange->y = ss->visibleRect.y;
        
        if(!VScroll_IsCursorVisible(ss, *rRange)){
            VScroll_FitCursorToRange(ss, ss->visibleRect, lineBuffer);
        }
        
        Float dif = Absf(lak - rRange->x);
        if(anim->is_down){
            if(ss->cursor.textPosition.x < oldP.x){
                ss->cursor.textPosition = oldP;
                VScroll_UpdateRelativeDistance(ss, oldP.x, lineBuffer->lineCount);
            }
            *transform = Translate(0, dif, 0);
        }else{
            if(ss->cursor.textPosition.x > oldP.x){
                ss->cursor.textPosition = oldP;
                VScroll_UpdateRelativeDistance(ss, oldP.x, lineBuffer->lineCount);
            }
            *transform = Translate(0, -dif, 0);
        }
        
        cursorAt->x = ss->cursor.textPosition.x;
        cursorAt->y = ss->cursor.textPosition.y;
        
        if(ss->visibleRect.x == anim->endLine) goto __set_end_transition;
    }
    
    return 0;
    __set_end_transition:
    ss->visibleRect.x = anim->endLine;
    ss->visibleRect.y = Min(ss->visibleRect.x + range, lineBuffer->lineCount);
    VScroll_FitCursorToRange(ss, ss->visibleRect, lineBuffer);
    cursorAt->x = ss->cursor.textPosition.x;
    cursorAt->y = ss->cursor.textPosition.y;
    rRange->x = ss->visibleRect.x;
    rRange->y = ss->visibleRect.y;
    
    anim->isAnimating = 0;
    *transform = Transform();
    return 1;
}

void VScroll_StartCursorTransition(VScroll *ss, uint lineNo, uint col,
                                   Float duration, LineBuffer *lineBuffer)
{
    AssertA(ss != nullptr, "Invalid scroll controller pointer");
    lineNo = Clamp(lineNo, (uint)0, lineBuffer->lineCount-1);
    if(IsZero(duration) || duration < 0){ // if no interval is given simply move
        VScroll_CursorToPosition(ss, lineNo, col, lineBuffer);
    }else if(ss->cursor.textPosition.x != lineNo){ // actual animation
        AssertA(ss->transitionAnim.isAnimating == 0, "Only one animation at a time");
        ss->transitionAnim.isAnimating = 1;
        ss->transitionAnim.transition = TransitionCursor;
        ss->transitionAnim.passedTime = 0;
        ss->transitionAnim.duration = duration;
        ss->transitionAnim.startLine = ss->cursor.textPosition.x;
        ss->transitionAnim.endLine = lineNo;
        ss->transitionAnim.endCol = col;
        ss->transitionAnim.runningPosition = (Float)ss->cursor.textPosition.x;
        ss->transitionAnim.is_down = ss->cursor.textPosition.x < lineNo;
        ss->transitionAnim.velocity = ss->transitionAnim.is_down ? 10 : -10;
    }else if(ss->cursor.textPosition.x == lineNo){ // change in col, no anim
        VScroll_CursorToPosition(ss, lineNo, col, lineBuffer);
    }
}


int VScroll_GetCursorTransition(VScroll *ss, Float dt, vec2ui *rRange,
                                vec2ui *cursorAt, Transform *transform,
                                LineBuffer *lineBuffer)
{
    int gap = 2;
    AnimationProps *anim = &ss->transitionAnim;
    AssertA(anim->transition == TransitionCursor, "Incorrect transition query");
    ss->cursor.is_dirty = 1;
    anim->passedTime += Max(0, dt);
    if(Animation_Finished(anim)){
        // This transition is done
        goto __set_end_transition;
    }else{
        Buffer *buffer = nullptr;
        vec2ui rect = ss->visibleRect;
        uint range = ss->currentMaxRange;
#if 0
        Float lak = InterpolateValueLinear(anim->passedTime, anim->duration,
                                           (Float)anim->startLine, (Float)anim->endLine);
#else
        Float remaining = anim->duration - anim->passedTime;
        Float lak = InterpolateValueCubic(dt, remaining, &anim->runningPosition,
                                          (Float)anim->endLine, &anim->velocity);
#endif
        cursorAt->x = (uint)Max(0, round(lak));
        buffer = LineBuffer_GetBufferAt(lineBuffer, cursorAt->x);
        if(buffer){
            uint initialp = 0;
            if(buffer->tokenCount > 0) initialp = buffer->tokens[0].position;
            cursorAt->y = Clamp(ss->cursor.textPosition.y, initialp, buffer->count-1);
        }
        
        if(anim->is_down){
            if(!(cursorAt->x < rect.y - gap)){
                rRange->y = Min(cursorAt->x + gap + 1, lineBuffer->lineCount-1);
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
                rRange->y = Min(rRange->x + range + 1, lineBuffer->lineCount-1);
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
    rRange->x = ss->visibleRect.x;
    rRange->y = ss->visibleRect.y;
    cursorAt->x = ss->cursor.textPosition.x;
    cursorAt->y = ss->cursor.textPosition.y;
    *transform = Transform();
    anim->isAnimating = 0;
    VScroll_CursorToPosition(ss, anim->endLine, anim->endCol, lineBuffer);
    return 1;
}
