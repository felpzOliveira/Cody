#include <bufferview.h>
#include <utilities.h>
#include <app.h>
#include <symbol.h>

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
    Float b2 = finalValue;
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

int Animation_Finished(AnimationProps *anim){
    return IsZero(anim->passedTime - anim->duration) || (anim->passedTime > anim->duration);
}

inline int BufferView_IsPositionVisible(BufferView *view, vec2ui position){
    VScroll *ss = &view->sController;
    return (ss->visibleRect.x < position.x && ss->visibleRect.y > position.x) ? 1 : 0;
}


void BufferView_AdjustGhostCursorIfOut(BufferView *view){
    VScroll_AdjustGhostIfNeeded(&view->sController, view->lineBuffer);
}

void BufferView_CursorTo(BufferView *view, uint lineNo){
    if(view->lineBuffer){
        VScroll_CursorTo(&view->sController, lineNo, view->lineBuffer);
    }
}

//TODO: Figure this out!
void BufferView_Synchronize(BufferView *view){
    if(view->lineBuffer){
        VScroll *ss = &view->sController;
        // 1- Check if our cursor is within allowed limits
        vec2ui cursor = ss->cursor.textPosition;
        cursor.x = Clamp(cursor.x, (uint)0, view->lineBuffer->lineCount-1);
        Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer, cursor.x);
        if(buffer == nullptr){
            BUG();
            printf("Null buffer inside a valid linebuffer line %p\n", view->lineBuffer);
            ss->cursor.textPosition = vec2ui(0);
            return;
        }

        cursor.y = Clamp(cursor.y, (uint)0, buffer->count);

        // 2- Update the scroll controller so that viewing range also gets updated
        VScroll_CursorToPosition(ss, cursor.x, cursor.y, view->lineBuffer);
    }else{ // our buffer got killed
        BufferView_SwapBuffer(view, nullptr);
    }
}

void BufferView_GhostCursorFollow(BufferView *view){
    VScroll_GhostCursorFollow(&view->sController);
}

int BufferView_LocateNextCursorToken(BufferView *view, Token **targetToken){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer,
                                            view->sController.cursor.textPosition.x);
    int tokenID = -1;
    uint pos = Buffer_Utf8PositionToRawPosition(buffer,
                                                view->sController.cursor.textPosition.y);
    if(pos < buffer->taken){
        for(int i = 0; i < (int)buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            if((token->position + token->size > (int)pos &&
               (token->identifier != TOKEN_ID_SPACE &&
                token->identifier != TOKEN_ID_MATH)) ||
               (i == (int)buffer->tokenCount-1 && token->identifier == TOKEN_ID_SPACE))
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
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer,
                                            view->sController.cursor.textPosition.x);
    int tokenID = -1;
    uint pos = Buffer_Utf8PositionToRawPosition(buffer, 
                                                view->sController.cursor.textPosition.y);
    if(pos > 0){
        for(int i = (int)buffer->tokenCount-1; i >= 0; i--){
            Token *token = &buffer->tokens[i];
            if((token->position < (int)pos && 
               (token->identifier != TOKEN_ID_SPACE &&
                token->identifier != TOKEN_ID_MATH)) || 
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

void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer){
    AssertA(view != nullptr, "Invalid initialization");
    view->lineBuffer = lineBuffer;
    view->renderLineNbs = 1;
    view->activeNestPoint = -1;
    view->scroll.currX = 0;
    VScroll_Init(&view->sController);
    view->scroll.horizontal = Transform();
}

void BufferView_SwapBuffer(BufferView *view, LineBuffer *lineBuffer){
    // We need to reset geometry because scroll actually holds visible range
    Float lineHeight = view->sController.lineHeight;
    view->lineBuffer = lineBuffer;
    view->scroll.currX = 0;
    view->activeNestPoint = -1;
    VScroll_Init(&view->sController);
    BufferView_SetGeometry(view, view->geometry, lineHeight);
}

void BufferView_SetGeometry(BufferView *view, Geometry geometry, Float lineHeight){
    Float height = geometry.upper.y - geometry.lower.y;
    view->geometry = geometry;
    view->sController.lineHeight = lineHeight;
    uint yRange = (uint)floor(height / view->sController.lineHeight) - 1;
    view->sController.currentMaxRange = yRange;
    view->sController.visibleRect.y = view->sController.visibleRect.x + yRange;
    if(view->lineBuffer){
        VScroll_CursorTo(&view->sController, view->sController.cursor.textPosition.x,
                         view->lineBuffer);
    }
}

vec2ui BufferView_GetViewRange(BufferView *view){
    return vec2ui(view->sController.visibleRect.x, 
                  Min(view->sController.visibleRect.y, view->lineBuffer->lineCount));
}

int BufferView_ComputeTextLine(BufferView *view, Float screenY, DescriptionLocation desc){
    if(view->lineBuffer){
        Float lStart = view->sController.visibleRect.x;
        if(desc == DescriptionTop){
            screenY -= view->sController.lineHeight;
        }
        
        Float fline = lStart + floor(screenY / view->sController.lineHeight);
        if((uint)fline < view->lineBuffer->lineCount){
            return (int)fline;
        }
    }
    return -1;
}

//TODO: It seems we have a bug when constantly pressing for this animation during
//      searching, debug it.
void BufferView_CursorToPosition(BufferView *view, uint lineNo, uint col){
    return VScroll_CursorToPosition(&view->sController, lineNo, col,
                                    view->lineBuffer);
}

void BufferView_StartScrollViewTransition(BufferView *view, int lineDiffs, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    return VScroll_StartScrollViewTransition(&view->sController, lineDiffs,
                                             duration, view->lineBuffer);
}

int BufferView_GetScrollViewTransition(BufferView *view, Float dt, vec2ui *rRange,
                                       vec2ui *cursorAt, Transform *transform)
{
    return VScroll_GetScrollViewTransition(&view->sController, dt, rRange,
                                           cursorAt, transform, view->lineBuffer);
}

void BufferView_StartNumbersShowTransition(BufferView *view, Float duration){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    if(view->renderLineNbs == 0){
        view->sController.transitionAnim.isAnimating = 1;
        view->sController.transitionAnim.transition = TransitionNumbers;
        view->sController.transitionAnim.passedTime = 0;
        view->sController.transitionAnim.duration = duration;
    }
}

int BufferView_GetNumbersShowTransition(BufferView *view, Float dt){
    AnimationProps *anim = &view->sController.transitionAnim;
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

void BufferView_StartCursorTransition(BufferView *view, uint lineNo,
                                      uint col, Float duration)
{
    AssertA(view != nullptr, "Invalid bufferview pointer");
    return VScroll_StartCursorTransition(&view->sController, lineNo, col,
                                         duration, view->lineBuffer);
}

int BufferView_GetCursorTransition(BufferView *view, Float dt, vec2ui *rRange,
                                   vec2ui *cursorAt, Transform *transform)
{
    AssertA(view != nullptr, "Invalid bufferview pointer");
    return VScroll_GetCursorTransition(&view->sController, dt, rRange,
                                       cursorAt, transform, view->lineBuffer);
}

Float BufferView_GetDescription(BufferView *view, char *content, uint size){
    vec2ui pos = VScroll_GetCursorPosition(&view->sController);
    uint lineCount = view->lineBuffer->lineCount;
    Float pct = ((Float) pos.x) / ((Float) lineCount);
    uint st = GetSimplifiedPathName(view->lineBuffer->filePath,
                                    view->lineBuffer->filePathSize);
    
    if(view->lineBuffer->is_dirty == 0){
        snprintf(content, size, " %s - Row: %u Col: %u", &view->lineBuffer->filePath[st],
                 pos.x+1, pos.y+1);
    }else{
        snprintf(content, size, " %s - Row: %u Col: %u *", &view->lineBuffer->filePath[st],
                 pos.x+1, pos.y+1);
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
    
    uint k = 0;
    uint bid = start.x;
    uint startAt = Buffer_GetTokenAt(buffer, start.y);
    int done = 0;
    int is_first = 1;
    uint zeroContext = 0;
    for(uint i = 0; i < nIds; i++){
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
                if(zeroContext > kMaximumIndentEmptySearch) done = 1;
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
    
    uint k = 0;
    int is_first = 1;
    uint bid = start.x;
    uint startAt = Buffer_GetTokenAt(buffer, start.y);
    int done = 0;
    uint zeroContext = 0;
    
    for(uint i = 0; i < nIds; i++){
        n[i] = 0;
    }
    
    do{
        for(int i = (int)startAt; i >= 0; i--){
            Token *token = &buffer->tokens[i];
            if(token && i < (int)buffer->tokenCount){
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
                if(zeroContext > kMaximumIndentEmptySearch) done = 1;
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
    return view->sController.cursor.nestValid;
}

void BufferView_UpdateCursorNesting(BufferView *view){
    VScroll *ss = &view->sController;
    if(ss->cursor.is_dirty){
        ss->cursor.is_dirty = 0;
        ss->cursor.nestValid = 0;
        
        int is_first = 1;
        vec2ui s = VScroll_GetCursorPosition(ss);
        
        TokenId ids[]  = { TOKEN_ID_BRACE_OPEN, TOKEN_ID_PARENTHESE_OPEN };
        TokenId cids[] = { TOKEN_ID_BRACE_CLOSE, TOKEN_ID_PARENTHESE_CLOSE };
        int n[] = {0, 0};
        int nc[] = {0, 0};
        view->startNestCount = BufferView_FindNestsBackwards(view, s, ids, cids, 2, 
                                                             view->startNest, 64, n);
        view->endNestCount = BufferView_FindNestsForward(view, s, ids, cids, 2,
                                                         view->endNest, 64, n);
        uint eid = 0;
        for(int i = 0; i < view->startNestCount; i++){
            TokenId idst = view->startNest[i].id;
            for(int k = eid; k < view->endNestCount; k++){
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
                        ss->cursor.nestStart = vec2ui(view->startNest[i].position.x,
                                                      view->startNest[i].position.y);
                        ss->cursor.nestEnd = vec2ui(view->endNest[k].position.x,
                                                    view->endNest[k].position.y);
                        ss->cursor.nestValid = 1;
                        is_first = 0;
                    }
                    break;
                }
            }
        }
        
        // set nest depth, have to do later because numbers are actually reversed
        // and we can't pre-compute.
        for(int i = 0; i < view->startNestCount; i++){
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

void BufferView_Dirty(BufferView *view){
    if(view){
        if(view->lineBuffer){
            view->lineBuffer->is_dirty = 1;
        }
    }
}

uint BufferView_GetCursorSelectionRange(BufferView *view, vec2ui *start, vec2ui *end){
    return VScroll_GetCursorSelection(&view->sController, start, end);
}

void BufferView_GetGeometry(BufferView *view, Geometry *geometry){
    *geometry = view->geometry;
}

void BufferView_GetCursor(BufferView *view, DoubleCursor **cursor){
    *cursor = &view->sController.cursor;
}

vec2ui BufferView_GetCursorPosition(BufferView *view){
    return view->sController.cursor.textPosition;
}

vec2ui BufferView_GetGhostCursorPosition(BufferView *view){
    return view->sController.cursor.ghostPosition;
}

int BufferView_IsAnimating(BufferView *view){
    return view->sController.transitionAnim.isAnimating;
}

uint BufferView_GetLineCount(BufferView *view){
    return view->lineBuffer->lineCount;
}

Transition BufferView_GetTransitionType(BufferView *view){
    return view->sController.transitionAnim.transition;
}

LineBuffer *BufferView_GetLineBuffer(BufferView *view){
    return view->lineBuffer;
}

Buffer *BufferView_GetBufferAt(BufferView *view, uint lineNo){
    return LineBuffer_GetBufferAt(view->lineBuffer, lineNo);
}
