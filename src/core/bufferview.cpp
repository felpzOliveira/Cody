#include <bufferview.h>
#include <utilities.h>
#include <app.h>
#include <symbol.h>

int Geometry_IsPointInside(Geometry *geometry, vec2ui p){
    return ((p.x <= geometry->upper.x && p.x >= geometry->lower.x) &&
            (p.y <= geometry->upper.y && p.y >= geometry->lower.y)) ? 1 : 0;
}

Float InterpolateValueCubic(Float dt, Float remaining,
                            Float *initialValue, Float finalValue, 
                            Float *velocity)
{
    if(dt > remaining){
        *initialValue = finalValue;
        return finalValue;
    }
    
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
    Float b2 = finalValue - (remaining / 3.0f) * (*velocity);
    Float b3 = finalValue;
    
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


inline int Animation_Finished(AnimationProps *anim){
    return IsZero(anim->passedTime - anim->duration) || (anim->passedTime > anim->duration);
}

inline int BufferView_IsCursorVisible(BufferView *view, vec2ui range){
    return (range.x <= view->cursor.textPosition.x &&
            range.y >= view->cursor.textPosition.x) ? 1 : 0;
}

void BufferView_AdjustGhostCursorIfOut(BufferView *view){
    Buffer *b = BufferView_GetBufferAt(view, view->cursor.ghostPosition.x);
    if(b == nullptr){ // in case there was a delete this will be nullptr
        BufferView_GhostCursorFollow(view);
    }else if(!(view->cursor.ghostPosition.y < b->count)){
        BufferView_GhostCursorFollow(view);
    }
}

//TODO: Add as needed
void BufferView_SynchronizeWith(BufferView *view, BufferView *source){
    // 1- Check if cursor is out of bounds
    vec2ui cursor = view->cursor.textPosition;
    vec2ui visible = BufferView_GetViewRange(view);
    int remap = 1;
    if(cursor.x >= visible.x && cursor.x < visible.y){
        Buffer *buffer = BufferView_GetBufferAt(view, cursor.x);
        if(buffer){
            if(cursor.y <= buffer->count){
                remap = 0;
            }
        }
    }
    
    if(remap){
        view->cursor = source->cursor;
    }
}

void BufferView_GhostCursorFollow(BufferView *view){
    view->cursor.ghostPosition = view->cursor.textPosition;
}

int BufferView_LocateNextCursorToken(BufferView *view, Token **targetToken){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer, view->cursor.textPosition.x);
    int tokenID = -1;
    uint pos = Buffer_Utf8PositionToRawPosition(buffer, view->cursor.textPosition.y);
    if(pos < buffer->taken){
        for(int i = 0; i < (int)buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            if(token->position + token->size > pos &&
               (token->identifier != TOKEN_ID_SPACE &&
                token->identifier != TOKEN_ID_MATH) ||
               (i == buffer->tokenCount-1 && token->identifier == TOKEN_ID_SPACE))
            {
                *targetToken = token;
                tokenID = i;
                break;
            }
        }
    }
    return tokenID;
}

int BufferView_LocatePreviousCursorToken(BufferView *view, Token **targetToken){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer, view->cursor.textPosition.x);
    int tokenID = -1;
    uint pos = Buffer_Utf8PositionToRawPosition(buffer, view->cursor.textPosition.y);
    if(pos > 0){
        for(int i = (int)buffer->tokenCount-1; i >= 0; i--){
            Token *token = &buffer->tokens[i];
            if(token->position < pos && 
               (token->identifier != TOKEN_ID_SPACE &&
                token->identifier != TOKEN_ID_MATH) || 
               (i == 0 && token->identifier == TOKEN_ID_SPACE))
            {
                *targetToken = token;
                tokenID = i;
                break;
            }
        }
    }
    return tokenID;
}

void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer,
                           Tokenizer *tokenizer)
{
    AssertA(view != nullptr && lineBuffer != nullptr && tokenizer != nullptr,
            "Invalid initialization");
    view->lineBuffer = lineBuffer;
    view->tokenizer = tokenizer;
    view->descLocation = DescriptionTop;
    view->cursor.textPosition  = vec2ui(0, 0);
    view->cursor.ghostPosition = vec2ui(0, 0);
    view->cursor.is_dirty = 1;
    view->cursor.nestStart = vec2ui(0, 0);
    view->cursor.nestEnd = vec2ui(0, 0);
    view->cursor.nestValid = 0;
    view->renderLineNbs = 0;
    view->activeNestPoint = -1;
    view->visibleRect = vec2ui(0, 0);
    view->lineHeight = 0;
    view->transitionAnim.transition = TransitionNone;
    view->transitionAnim.isAnimating = 0;
    view->transitionAnim.duration = 0.0f;
    view->transitionAnim.endLine = 0;
    view->transitionAnim.velocity = 0;
}

void BufferView_SetGeometry(BufferView *view, Geometry geometry, Float lineHeight){
    Float height = geometry.upper.y - geometry.lower.y;
    view->geometry = geometry;
    view->lineHeight = lineHeight;
    uint yRange = (uint)floor(height / view->lineHeight) - 1;
    view->currentMaxRange = yRange;
    view->visibleRect.y = view->visibleRect.x + yRange;
    BufferView_CursorTo(view, view->cursor.textPosition.x);
}

vec2ui BufferView_GetViewRange(BufferView *view){
    return vec2ui(view->visibleRect.x, Min(view->visibleRect.y,
                                           view->lineBuffer->lineCount));
}

void BufferView_FitCursorToRange(BufferView *view, vec2ui range){
    Buffer *buffer = nullptr;
    vec2ui cCoord = view->cursor.textPosition;
    int X = (int)range.x;
    int Y = (int)range.y;
    int lineNo = (int)cCoord.x;
    int gap = 2;
    if(!(lineNo < Y - gap && lineNo > X + gap)){
        if(lineNo < X + gap){
            lineNo = Min(view->lineBuffer->lineCount-1, X + gap);
        }else if(lineNo > Y - gap){
            lineNo = Max(Y - gap, 0);
        }
    }
    
    buffer = LineBuffer_GetBufferAt(view->lineBuffer, lineNo);
    
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
    
    view->cursor.is_dirty = 1;
}

void BufferView_CursorTo(BufferView *view, uint lineNo){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    int gap = 1;
    vec2ui visibleRect = view->visibleRect;
    uint lineRangeSize = view->currentMaxRange;
    if(!(lineNo < visibleRect.y-gap && lineNo > visibleRect.x+gap)){
        // Outside screen
        if(lineNo <= visibleRect.x + gap && visibleRect.x != 0){ // going up
            int iLine = (int)lineNo;
            iLine = Max(0, iLine - gap);
            visibleRect.x = (uint)iLine;
            visibleRect.y = Min(visibleRect.x + lineRangeSize,
                                view->lineBuffer->lineCount);
        }else if(lineNo >= visibleRect.y - gap){ // going down
            visibleRect.y = Min(lineNo + gap + 1, view->lineBuffer->lineCount);
            visibleRect.x = visibleRect.y - lineRangeSize;
        }
    }
    
    view->visibleRect = visibleRect;
    if(lineNo < view->lineBuffer->lineCount){
        view->cursor.textPosition.x = lineNo;
    }
    
    view->cursor.is_dirty = 1;
}

int BufferView_ComputeTextLine(BufferView *view, Float screenY){
    Buffer *buffer = nullptr;
    Float lStart = view->visibleRect.x;
    if(view->descLocation == DescriptionTop){
        screenY -= view->lineHeight;
    }
    
    Float fline = lStart + floor(screenY / view->lineHeight);
    if((uint)fline < view->lineBuffer->lineCount){
        return (int)fline;
    }
    
    return -1;
}

void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col){
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer, (uint)lineNo);
    if(buffer){
        if(buffer->count > 0)
            col = Clamp(col, 0, buffer->count);
        else
            col = 0;
        
        BufferView_CursorTo(view, (uint)lineNo);
        view->cursor.textPosition.y = col;
        
    }
}

void BufferView_StartScrollViewTransition(BufferView *view, int lineDiffs, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    //AssertA(view->transitionAnim.isAnimating == 0, "Only one animation at a time");
    
    int expectedEnd = (int)view->visibleRect.x + lineDiffs;
    if(view->transitionAnim.isAnimating == 0){
        if(lineDiffs < 0){ // going up
            expectedEnd = Max(0, expectedEnd);
        }else{ // going down
            expectedEnd = Min(view->lineBuffer->lineCount - 1, expectedEnd);
        }
        
        view->transitionAnim.isAnimating = 1;
        view->transitionAnim.transition = TransitionScroll;
        view->transitionAnim.passedTime = 0;
        view->transitionAnim.duration = duration;
        view->transitionAnim.endLine = expectedEnd;
        view->transitionAnim.velocity = 0;
        view->transitionAnim.startLine = view->visibleRect.x;
        view->transitionAnim.runningPosition = (Float)view->visibleRect.x;
        view->transitionAnim.is_down = lineDiffs > 0;
    }else if(view->transitionAnim.transition == TransitionScroll){
        expectedEnd = view->transitionAnim.endLine + lineDiffs;
        if(lineDiffs < 0){ // going up
            expectedEnd = Max(0, expectedEnd);
        }else{ // going down
            expectedEnd = Min(view->lineBuffer->lineCount - 1, expectedEnd);
        }
        
        int is_down = expectedEnd > view->transitionAnim.endLine;
        view->transitionAnim.endLine = expectedEnd;
        view->transitionAnim.is_down = is_down;
    }
}

int BufferView_GetScrollViewTransition(BufferView *view, Float dt, vec2ui *rRange,
                                       vec2ui *cursorAt, Transform *transform)
{
    AnimationProps *anim = &view->transitionAnim;
    vec2ui oldP = view->cursor.textPosition;
    vec2ui vRect = view->visibleRect;
    uint range = view->currentMaxRange;
    
    AssertA(anim->transition == TransitionScroll, "Incorrect transition query");
    
    view->cursor.is_dirty = 1;
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
        view->visibleRect.x = Max(0, Floor(lak));
        view->visibleRect.y = Min(view->visibleRect.x + range,
                                  view->lineBuffer->lineCount);
        
        rRange->x = view->visibleRect.x;
        rRange->y = view->visibleRect.y;
        
        if(!BufferView_IsCursorVisible(view, *rRange)){
            BufferView_FitCursorToRange(view, view->visibleRect);
        }
        
        Float dif = Absf(lak - rRange->x);
        if(anim->is_down){
            if(view->cursor.textPosition.x < oldP.x){
                view->cursor.textPosition = oldP;
            }
            *transform = Translate(0, dif, 0);
        }else{
            if(view->cursor.textPosition.x > oldP.x){
                view->cursor.textPosition = oldP;
            }
            *transform = Translate(0, -dif, 0);
        }
        
        cursorAt->x = view->cursor.textPosition.x;
        cursorAt->y = view->cursor.textPosition.y;
        
        if(view->visibleRect.x == anim->endLine) goto __set_end_transition;
    }
    
    return 0;
    __set_end_transition:
    view->visibleRect.x = anim->endLine;
    view->visibleRect.y = Min(view->visibleRect.x + range,
                              view->lineBuffer->lineCount);
    BufferView_FitCursorToRange(view, view->visibleRect);
    cursorAt->x = view->cursor.textPosition.x;
    cursorAt->y = view->cursor.textPosition.y;
    rRange->x = view->visibleRect.x;
    rRange->y = view->visibleRect.y;
    
    anim->isAnimating = 0;
    *transform = Transform();
    return 1;
}

void BufferView_StartNumbersShowTransition(BufferView *view, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    if(view->renderLineNbs == 0){
        view->transitionAnim.isAnimating = 1;
        view->transitionAnim.transition = TransitionNumbers;
        view->transitionAnim.passedTime = 0;
        view->transitionAnim.duration = duration;
    }
}

int BufferView_GetNumbersShowTransition(BufferView *view, Float dt){
    AnimationProps *anim = &view->transitionAnim;
    AssertA(anim->transition == TransitionNumbers, "Incorrect transition query");
    anim->passedTime += Max(0, dt);
    if(anim->passedTime < anim->duration){
        view->renderLineNbs = 1;
    }else{
        view->renderLineNbs = 0;
    }
    
    if(view->renderLineNbs == 0){ // done
        anim->isAnimating = 0;
    }
    
    return view->renderLineNbs == 0 ? 1 : 0;
}

void BufferView_StartCursorTransition(BufferView *view, uint lineNo, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    lineNo = Clamp(lineNo, 0, view->lineBuffer->lineCount-1);
    if(IsZero(duration) || duration < 0){ // if no interval is given simply move
        BufferView_CursorTo(view, lineNo);
    }else if(view->cursor.textPosition.x != lineNo){ // actual animation
        AssertA(view->transitionAnim.isAnimating == 0, "Only one animation at a time");
        view->transitionAnim.isAnimating = 1;
        view->transitionAnim.transition = TransitionCursor;
        view->transitionAnim.passedTime = 0;
        view->transitionAnim.duration = duration;
        view->transitionAnim.startLine = view->cursor.textPosition.x;
        view->transitionAnim.endLine = lineNo;
        view->transitionAnim.velocity = 0;
        view->transitionAnim.runningPosition = (Float)view->cursor.textPosition.x;
        view->transitionAnim.is_down = view->cursor.textPosition.x < lineNo;
    }else{
        //AssertA(0, "Invalid transition requested");
    }
}

int BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                   vec2ui *cursorAt, Transform *transform)
{
    int gap = 2;
    AnimationProps *anim = &view->transitionAnim;
    AssertA(anim->transition == TransitionCursor, "Incorrect transition query");
    view->cursor.is_dirty = 1;
    anim->passedTime += Max(0, dt);
    if(Animation_Finished(anim)){
        // This transition is done
        goto __set_end_transition;
    }else{
        Buffer *buffer = nullptr;
        vec2ui rect = view->visibleRect;
        uint range = view->currentMaxRange;
#if 0
        Float lak = InterpolateValueLinear(anim->passedTime, anim->duration,
                                           (Float)anim->startLine, (Float)anim->endLine);
#else
        Float remaining = anim->duration - anim->passedTime;
        Float lak = InterpolateValueCubic(dt, remaining, &anim->runningPosition,
                                          (Float)anim->endLine, &anim->velocity);
#endif
        cursorAt->x = (uint)Max(0, round(lak));
        buffer = LineBuffer_GetBufferAt(view->lineBuffer, cursorAt->x);
        if(buffer){
            uint initialp = 0;
            if(buffer->tokenCount > 0) initialp = buffer->tokens[0].position;
            cursorAt->y = Clamp(view->cursor.textPosition.y,initialp, buffer->count-1);
        }
        
        if(anim->is_down){
            if(!(cursorAt->x < rect.y - gap)){
                rRange->y = Min(cursorAt->x + gap + 1, view->lineBuffer->lineCount-1);
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
                rRange->y = Min(rRange->x + range + 1, view->lineBuffer->lineCount-1);
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

Float BufferView_GetDescription(BufferView *view, char *content, uint size){
    uint lineCount = view->lineBuffer->lineCount;
    Float pct = ((Float) view->cursor.textPosition.x) / ((Float) lineCount);
    uint st = GetSimplifiedPathName(view->lineBuffer->filePath,
                                    view->lineBuffer->filePathSize);
    
    if(view->lineBuffer->is_dirty == 0){
        snprintf(content, size, " %s - Row: %u Col: %u", &view->lineBuffer->filePath[st],
                 view->cursor.textPosition.x+1, view->cursor.textPosition.y+1);
    }else{
        snprintf(content, size, " %s - Row: %u Col: %u *", &view->lineBuffer->filePath[st],
                 view->cursor.textPosition.x+1, view->cursor.textPosition.y+1);
    }
    
    return pct;
}

void BufferView_ToogleLineNumbers(BufferView *view){
    view->renderLineNbs = 1 - view->renderLineNbs;
}

int BufferView_FindNestsForward(BufferView *view, vec2ui start, TokenId *ids,
                                TokenId *cids, uint nIds, NestPoint *out, uint maxN,
                                int *n)
{
    Buffer *buffer = BufferView_GetBufferAt(view, start.x);
    if(buffer == nullptr){ return 0; }
    
    int k = 0;
    uint len = DefaultAllocatorSize;
    uint bid = start.x;
    uint startAt = Buffer_GetTokenAt(buffer, start.y);
    int done = 0;
    int is_first = 1;
    uint zeroContext = 0;
    for(int i = 0; i < nIds; i++){
        n[i] = 0;
    }
    
    do{
        int r = (int)buffer->tokenCount;
        for(int i = (int)startAt; i < r; i++){
            Token *token = &buffer->tokens[i];
            if(token){
                for(uint s = 0; s < nIds; s++){
                    if(token->identifier == cids[s] && n[s] == 0){
                        if(!(k < maxN)) return k;
                        out[k].position = vec2ui(bid, i);
                        out[k].valid = 0;
                        out[k].id = cids[s];
                        k++;
                    }else if(token->identifier == ids[s]){
                        if(!(is_first)){
                            n[s]--;
                        }
                    }else if(token->identifier == cids[s]){
                        n[s]++;
                    }
                }
            }
            
            is_first = 0;
        }
        
        bid += 1;
        if(bid < view->lineBuffer->lineCount){
            buffer = BufferView_GetBufferAt(view, bid);
            startAt = 0;
            
            TokenizerStateContext *ctx = &buffer->stateContext;
            if(ctx->indentLevel == 0){
                zeroContext++;
                if(zeroContext > 1) done = 1;
            }else{
                zeroContext = 0;
            }
        }else{
            done = 1;
        }
        
    }while(done == 0);
    
    return k;
}

int BufferView_FindNestsBackwards(BufferView *view, vec2ui start, TokenId *ids,
                                  TokenId *cids, uint nIds, NestPoint *out, uint maxN,
                                  int *n)
{
    Buffer *buffer = BufferView_GetBufferAt(view, start.x);
    if(buffer == nullptr){ return 0; }
    
    int k = 0;
    int is_first = 1;
    uint bid = start.x;
    uint startAt = Buffer_GetTokenAt(buffer, start.y);
    int done = 0;
    uint zeroContext = 0;
    
    for(int i = 0; i < nIds; i++){
        n[i] = 0;
    }
    
    do{
        for(int i = (int)startAt; i >= 0; i--){
            Token *token = &buffer->tokens[i];
            if(token && i < buffer->tokenCount){
                for(uint s = 0; s < nIds; s++){
                    if(token->identifier == ids[s] && n[s] == 0){
                        if(!(k < maxN)) return k;
                        out[k].position = vec2ui(bid, i);
                        out[k].valid = 0;
                        out[k].id = ids[s];
                        k++;
                    }else if(token->identifier == cids[s]){
                        if(!(is_first)){
                            n[s]--;
                        }
                    }else if(token->identifier == ids[s]){
                        n[s]++;
                    }
                }
            }
            
            is_first = 0;
        }
        
        if(bid > 0){
            bid = bid > 0 ? bid - 1 : 0;
            buffer = BufferView_GetBufferAt(view, bid);
            startAt = buffer->tokenCount > 0 ? buffer->tokenCount-1 : 0;
            TokenizerStateContext *ctx = &buffer->stateContext;
            if(ctx->indentLevel == 0){
                zeroContext++;
                if(zeroContext > 1) done = 1;
            }else{
                zeroContext = 0;
            }
        }else{
            done = 1;
        }
    }while(done == 0);
    
    return k;
}

int BufferView_CursorNestIsValid(BufferView *view){
    return view->cursor.nestValid;
    //return view->startNestCount > 0 && view->endNestCount;
}

void BufferView_UpdateCursorNesting(BufferView *view){
    if(view->cursor.is_dirty){
        view->cursor.is_dirty = 0;
        view->cursor.nestValid = 0;
        
        int is_first = 1;
        vec2ui s = view->cursor.textPosition;
        
        TokenId ids[]  = { TOKEN_ID_BRACE_OPEN, TOKEN_ID_PARENTHESE_OPEN };
        TokenId cids[] = { TOKEN_ID_BRACE_CLOSE, TOKEN_ID_PARENTHESE_CLOSE };
        int n[] = {0, 0};
        int nc[] = {0, 0};
        view->startNestCount = BufferView_FindNestsBackwards(view, s, ids, cids, 2, 
                                                             view->startNest, 64, n);
        view->endNestCount = BufferView_FindNestsForward(view, s, ids, cids, 2,
                                                         view->endNest, 64, n);
        uint eid = 0;
        for(uint i = 0; i < view->startNestCount; i++){
            TokenId idst = view->startNest[i].id;
            for(uint k = eid; k < view->endNestCount; k++){
                TokenId idend = view->endNest[k].id;
                if(Symbol_AreTokensComplementary(idst, idend)){
                    view->startNest[i].comp = k;
                    view->endNest[k].comp = i;
                    view->startNest[i].valid = 1;
                    view->endNest[i].valid = 1;
                    
                    // accumulate token information so we can output depth
                    int id = idst == TOKEN_ID_BRACE_OPEN ? 0 : 1;
                    nc[id]++;
                    
                    eid = k + 1;
                    if(is_first){ // the first one is the closest to the cursor
                        view->cursor.nestStart = vec2ui(view->startNest[i].position.x,
                                                        view->startNest[i].position.y);
                        view->cursor.nestEnd = vec2ui(view->endNest[k].position.x,
                                                      view->endNest[k].position.y);
                        view->cursor.nestValid = 1;
                        is_first = 0;
                    }
                    break;
                }
            }
        }
        
        // set nest depth, have to do later because numbers are actually reversed
        // and we can't pre-compute.
        for(uint i = 0; i < view->startNestCount; i++){
            if(view->startNest[i].valid){
                int id = view->startNest[i].id == TOKEN_ID_BRACE_OPEN ? 0 : 1;
                int k = view->startNest[i].comp;
                view->startNest[i].depth = nc[id] - 1;
                view->endNest[k].depth = nc[id] - 1;
                nc[id]--;
            }
        }
#if 0
        int k = view->startNestCount;
        if(k > 0){
            for(int i = 0; i < k; i++){
                NestPoint v = view->startNest[i];
                if(v.valid){
                    printf("Found %s at : %d %d, depth: %d ( %d )\n",
                           Symbol_GetIdString(v.id), v.position.x, v.position.y, v.depth,
                           v.comp);
                }
            }
        }
        
        k = view->endNestCount;
        printf("============================\n");
        if(k > 0){
            for(int i = 0; i < k; i++){
                NestPoint v = view->endNest[i];
                if(v.valid){
                    printf("Found %s at : %d %d, depth: %d ( %d )\n",
                           Symbol_GetIdString(v.id), v.position.x, v.position.y, 
                           v.depth, v.comp);
                }
            }
        }
        
        printf("Single: [%u %u] [%u %u]\n", view->cursor.nestStart.x,
               view->cursor.nestStart.y, view->cursor.nestEnd.x,
               view->cursor.nestEnd.y);
#endif
    }
}

uint BufferView_GetCursorSelectionRange(BufferView *view, vec2ui *start, vec2ui *end){
    vec2ui s = view->cursor.textPosition;
    vec2ui e = view->cursor.ghostPosition;
    
    if(s.x > e.x){ // ghost cursor is behind
        s = view->cursor.ghostPosition;
        e = view->cursor.textPosition;
    }else if(s.x == e.x){ // same line
        if(s.y == e.y){ // range is 0
            return 0;
        }
        if(s.y > e.y){ // cursor is foward in line
            s = view->cursor.ghostPosition;
            e = view->cursor.textPosition;
        }
    }
    
    *start = s;
    *end = e;
    return 1;
}

void BufferView_GetCursor(BufferView *view, DoubleCursor **cursor){
    *cursor = &view->cursor;
}

vec2ui BufferView_GetCursorPosition(BufferView *view){
    return view->cursor.textPosition;
}

vec2ui BufferView_GetGhostCursorPosition(BufferView *view){
    return view->cursor.ghostPosition;
}

int BufferView_IsAnimating(BufferView *view){
    return view->transitionAnim.isAnimating;
}

uint BufferView_GetLineCount(BufferView *view){
    return view->lineBuffer->lineCount;
}

Transition BufferView_GetTransitionType(BufferView *view){
    return view->transitionAnim.transition;
}

LineBuffer *BufferView_GetLineBuffer(BufferView *view){
    return view->lineBuffer;
}

Buffer *BufferView_GetBufferAt(BufferView *view, uint lineNo){
    return LineBuffer_GetBufferAt(view->lineBuffer, lineNo);
}
