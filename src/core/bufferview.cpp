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

Float InterpolateValueLinear(Float dt, Float remaining,
                             Float *initialValue, Float finalValue,
                             Float *velocity)
{
    if(dt > remaining){
        *initialValue = finalValue;
        return finalValue;
    }

    *velocity = (finalValue - *initialValue) / remaining;
    *initialValue = dt * (*velocity) + *initialValue;
    return *initialValue;
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

void BufferView_FitCursorToRange(BufferView *view, vec2ui range){
    if(view->lineBuffer){
        VScroll_FitCursorToRange(&view->sController, range, view->lineBuffer);
    }
}

//TODO: Figure this out!
void BufferView_Synchronize(BufferView *view){
    if(view->lineBuffer){
        VScroll *ss = &view->sController;
        // 0 - check for locks
        int is_locked = 0;
        LockedLineBuffer *lockedBuffer = nullptr;
        GetExecutorLockedLineBuffer(&lockedBuffer);
        if(lockedBuffer->lineBuffer == view->lineBuffer){
            lockedBuffer->mutex.lock();
            is_locked = 1;
        }
        // 1- Check if our cursor is within allowed limits
        vec2ui cursor = ss->cursor.textPosition;
        cursor.x = Clamp(cursor.x, (uint)0, view->lineBuffer->lineCount-1);
        Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer, cursor.x);
        if(buffer == nullptr){
            // internal buffers can be dynamically updated and might have a 0
            // line size. File linebuffer however can't.
            if(!LineBuffer_IsInternal(view->lineBuffer)){
                BUG();
                printf("Null buffer inside a valid linebuffer line %p\n", view->lineBuffer);
                printf("Cursor is located at %u, but buffer does not exists\n", cursor.x);
                printf("Linebuffer claims to have %d line(s)\n",
                        (int)view->lineBuffer->lineCount);
            }
            ss->cursor.textPosition = vec2ui(0);
            if(is_locked){
                lockedBuffer->mutex.unlock();
            }
            return;
        }

        cursor.y = Clamp(cursor.y, (uint)0, buffer->count);

        // 2- Update the scroll controller so that viewing range also gets updated
        VScroll_CursorToPosition(ss, cursor.x, cursor.y, view->lineBuffer);

        // adjust ghost cursor if needed
        VScroll_AdjustGhostIfNeeded(ss, view->lineBuffer);
        if(is_locked){
            lockedBuffer->mutex.unlock();
        }
    }else{ // our buffer got killed
        // TODO: do we need this?
        //BufferView_SwapBuffer(view, nullptr, EmptyView);
    }
}

void BufferView_GhostCursorFollow(BufferView *view){
    VScroll_GhostCursorFollow(&view->sController);
}

int BufferView_LocateNextCursorToken(BufferView *view, Token **targetToken){
    AssertA(view != nullptr, "Invalid bufferview pointer");
    Buffer *buffer = LineBuffer_GetBufferAt(view->lineBuffer,
                                            view->sController.cursor.textPosition.x);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    int tokenID = -1;
    uint pos = Buffer_Utf8PositionToRawPosition(buffer,
                                                view->sController.cursor.textPosition.y,
                                                nullptr, encoder);
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
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    uint pos = Buffer_Utf8PositionToRawPosition(buffer,
                                                view->sController.cursor.textPosition.y,
                                                nullptr, encoder);
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

void BufferView_SetPdfView(BufferView *view, PdfViewState *pdfView){
    if(view->pdfView){
        if(view->pdfView == pdfView)
            return;

        PdfView_CloseDocument(&view->pdfView);
    }

    view->pdfView = pdfView;
}

PdfViewState *BufferView_GetPdfView(BufferView *view){
    return view->pdfView;
}

void BufferView_Initialize(BufferView *view, LineBuffer *lineBuffer, ViewType type){
    AssertA(view != nullptr, "Invalid initialization");
    view->lineBuffer = lineBuffer;
    view->renderLineNbs = 1;
    view->activeNestPoint = -1;
    view->scroll.currX = 0;
    VScroll_Init(&view->sController);
    view->scroll.horizontal = Transform();
    view->is_visible = 1;
    view->activeType = type;
    view->is_mouse_pressed = 0;
    view->is_range_selected = 0;
    view->highlightLine = {};
    view->pdfView = nullptr;
    view->imgRenderer = ImageRenderer();
    view->scrollImgRenderer = ImageRenderer();
}


void BufferView_ResetImageRenderer(BufferView *view, PdfRenderPages pages){
    ImageRendererCleanup(view->imgRenderer);
    ImageRendererCleanup(view->scrollImgRenderer);
    ImageRendererCreate(view->imgRenderer, pages);
    ImageRendererCreate(view->scrollImgRenderer, pages);
}

void BufferView_SwapBuffer(BufferView *view, LineBuffer *lineBuffer, ViewType type){
    // We need to reset geometry because scroll actually holds visible range
    BufferViewFileLocation_Register(view);
    Float lineHeight = view->sController.lineHeight;
    view->lineBuffer = lineBuffer;
    view->scroll.currX = 0;
    view->activeNestPoint = -1;
    view->activeType = type;
    view->is_mouse_pressed = 0;
    view->is_range_selected = 0;
    view->highlightLine = {};
    VScroll_Init(&view->sController);
    BufferView_SetGeometry(view, view->geometry, lineHeight);
    BufferViewFileLocation_Restore(view);
    if(view->pdfView){
        PdfView_CloseDocument(&view->pdfView);
        view->pdfView = nullptr;
    }
}

uint BufferView_GetMaximumLineView(BufferView *view){
    uint n = 0;
    if(view){
        n = view->sController.currentMaxRange;
    }

    return n;
}

std::optional<LineHints> BufferView_GetLineHighlight(BufferView *view){
    AssertA(view != nullptr, "Invalid bufferview");
    return view->highlightLine;
}

void BufferView_ClearHighlightLine(BufferView *view){
    AssertA(view != nullptr, "Invalid bufferview");
    view->highlightLine = {};
}

void BufferView_SetHighlightLine(BufferView *view, LineHints hint){
    AssertA(view != nullptr, "Invalid bufferview");
    if(view->lineBuffer != nullptr){
        hint.line = Min(hint.line, view->lineBuffer->lineCount-1);
        view->highlightLine = std::optional<LineHints>(hint);
    }else{
        view->highlightLine = {};
    }
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

void BufferView_SetViewRange(BufferView *view, vec2ui range){
    if(view->lineBuffer){
        VScroll_SetViewRange(&view->sController, range, view->lineBuffer);
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

Float BufferView_GetDescription(BufferView *view, char *content, uint size,
                                char *endDesc, uint endSize)
{
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(view);
    uint elen = 0;
    vec2ui pos = VScroll_GetCursorPosition(&view->sController);
    uint lineCount = view->lineBuffer->lineCount;
    Float pct = ((Float) pos.x) / ((Float) lineCount);
    uint st = GetSimplifiedPathName(view->lineBuffer->filePath,
                                    view->lineBuffer->filePathSize);

    char *name = (char *)&view->lineBuffer->filePath[st];
    if(view->lineBuffer->filePathSize == 0){
        name = (char *)"Command";
    }

    if(view->lineBuffer->is_dirty == 0){
        snprintf(content, size, " %s - Row: %u Col: %u", name, pos.x+1, pos.y+1);
    }else{
        snprintf(content, size, " %s - Row: %u Col: %u *", name, pos.x+1, pos.y+1);
    }

    std::string path = std::string(view->lineBuffer->filePath);
    if(!AppIsPathFromRoot(path)){
        path = AppGetRootDirectory() + std::string("/") + path;
    }

    if(lineBuffer){
        // read-write state of the linebuffer
        if(!LineBuffer_IsWrittable(lineBuffer)){
            elen = snprintf(endDesc, endSize, "○ ");
            //elen = snprintf(endDesc, endSize, "ʀ ");
        }else{
            elen = snprintf(endDesc, endSize, "● ");
            //elen = snprintf(endDesc, endSize, "ʀԝ ");
        }
    }

    if(AppIsStoredFile(path)){
        snprintf(&endDesc[elen], endSize-elen, "†");
    }

    return pct;
}

void BufferView_ToogleLineNumbers(BufferView *view){
    view->renderLineNbs = 1 - view->renderLineNbs;
}

void BufferView_SetAnimationCallback(BufferView *view, std::function<void()> fn){
    if(view){
        VScroll_SetAnimationFinishCallback(&view->sController, fn);
    }
}

bool BufferView_FindFirstForward(BufferView *view, TokenId id, TokenId cid,
                                 vec2ui start, vec2ui &end)
{
    Buffer *buffer = BufferView_GetBufferAt(view, start.x);
    if(!buffer) return false;
    bool found = false;
    int done = 0;
    int depth = 0;
    uint bid = start.x;
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    uint startAt = Buffer_GetTokenAt(buffer, bid, encoder);

    do{
        int r = (int)buffer->tokenCount;
        for(int i = (int)startAt; i < r; i++){
            Token *token = &buffer->tokens[i];
            if(token){
                if(token->identifier == id){
                    if(depth == 0){
                        end.x = start.x;
                        end.y = i;
                        found = true;
                        done = 1;
                        break;
                    }else{
                        depth--;
                    }
                }else if(token->identifier == cid){
                    depth++;
                }
            }
        }

        bid += 1;
        if(bid < view->lineBuffer->lineCount){
            buffer = BufferView_GetBufferAt(view, bid);
            startAt = 0;
        }else{
            done = 1;
        }
    }while(done == 0);

    return found;
}

int BufferView_FindNestsForward(BufferView *view, vec2ui start, TokenId *ids,
                                TokenId *cids, uint nIds, NestPoint *out, uint maxN,
                                int *n)
{
    if(LineBuffer_IsInternal(view->lineBuffer)) return 0;
    Buffer *buffer = BufferView_GetBufferAt(view, start.x);
    if(buffer == nullptr){ return 0; }

    uint k = 0;
    uint bid = start.x;
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    uint startAt = Buffer_GetTokenAt(buffer, start.y, encoder);
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
    if(LineBuffer_IsInternal(view->lineBuffer)) return 0;
    Buffer *buffer = BufferView_GetBufferAt(view, start.x);
    if(buffer == nullptr){ return 0; }

    uint k = 0;
    int is_first = 1;
    uint bid = start.x;
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    uint startAt = Buffer_GetTokenAt(buffer, start.y, encoder);
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

int BufferView_IsVisible(BufferView *view){
    if(view){
        return view->is_visible;
    }

    return 0;
}

uint BufferView_GetCursorSelectionRange(BufferView *view, vec2ui *start, vec2ui *end){
    return VScroll_GetCursorSelection(&view->sController, start, end);
}

ViewType BufferView_GetViewType(BufferView *view){
    return view->activeType;
}

void BufferView_SetViewType(BufferView *view, ViewType type){
    view->activeType = type;
}

void BufferView_GetGeometry(BufferView *view, Geometry *geometry){
    *geometry = view->geometry;
}

void BufferView_GetCursor(BufferView *view, DoubleCursor **cursor){
    *cursor = &view->sController.cursor;
}

void BufferView_SetMousePressed(BufferView *view, int pressed){
    view->is_mouse_pressed = pressed;
}

void BufferView_SetRangeVisible(BufferView *view, int visible){
    view->is_range_selected = visible;
}

int BufferView_GetRangeVisible(BufferView *view){
    return view->is_range_selected;
}

int BufferView_GetMousePressed(BufferView *view){
    return view->is_mouse_pressed;
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

ImageRenderer *BufferView_GetImageRenderer(BufferView *view){
    return &view->imgRenderer;
}

LineBuffer *BufferView_GetLineBuffer(BufferView *view){
    return view->lineBuffer;
}

Buffer *BufferView_GetBufferAt(BufferView *view, uint lineNo){
    return LineBuffer_GetBufferAt(view->lineBuffer, lineNo);
}

//TODO: check if we can do faster than this
//      i don't think there is a problem because most usage is with 2 views
//      and we only depend on the hash map, but we can improve if it gets slow
struct BufferViewLocation{
    std::map<LineBuffer *, BufferViewFileLocation> locationMap;
    BufferView *view;
};

#define MAX_BUFFERVIEWS 16
BufferViewLocation globalLocations[MAX_BUFFERVIEWS];
int globalTakenPositions = 0;

inline
BufferViewLocation *BufferViewLocation_GetEntry(BufferView *view, int *at=nullptr){
    if(!view) return nullptr;
    BufferViewLocation *loc = nullptr;
    for(int i = 0; i < globalTakenPositions; i++){
        if(globalLocations[i].view == view){
            loc = &globalLocations[i];
            if(at) *at = i;
            break;
        }
    }

    return loc;
}

void BufferViewLocation_RemoveView(BufferView *view){
    BufferViewLocation *loc = nullptr;
    int at = -1;
    if(!view || globalTakenPositions < 1) return;
    loc = BufferViewLocation_GetEntry(view, &at);

    if(!loc || at < 0) return;

    // TODO: this is one expensive shift, but hopefully it wont
    //       be executed too much
    for(int i = at+1; i < globalTakenPositions; i++){
        BufferViewLocation *next = &globalLocations[i];
        loc->view = next->view;
        loc->locationMap = next->locationMap;
        next->view = nullptr;
        next->locationMap.clear();
    }

    globalTakenPositions -= 1;
}

void BufferViewLocation_RemoveLineBuffer(LineBuffer *lineBuffer){
    if(!lineBuffer || globalTakenPositions < 1) return;
    for(int i = 0; i < globalTakenPositions; i++){
        BufferViewLocation *loc = &globalLocations[i];
        if(loc->locationMap.find(lineBuffer) != loc->locationMap.end()){
            loc->locationMap.erase(lineBuffer);
            //printf("[DEBUG] Removed %s from map\n", lineBuffer->filePath);
        }
    }
}

void BufferViewFileLocation_Restore(BufferView *view){
    if(!view) return;
    if(!view->lineBuffer) return;

    BufferViewFileLocation location;
    LineBuffer *lineBuffer = view->lineBuffer;

    BufferViewLocation *loc = BufferViewLocation_GetEntry(view);
    if(!loc) return;

    if(loc->locationMap.find(lineBuffer) != loc->locationMap.end()){
        location = loc->locationMap[lineBuffer];
        vec2ui p = location.textPosition;
        BufferView_SetViewRange(view, location.range);
        BufferView_CursorToPosition(view, p.x, p.y);
        BufferView_GhostCursorFollow(view);
    }
}

void BufferViewFileLocation_Register(BufferView *view){
    LineBuffer *lineBuffer = nullptr;
    DoubleCursor *cursor = nullptr;
    BufferViewFileLocation location;

    if(!view) return;
    if(!view->lineBuffer) return;

    lineBuffer = view->lineBuffer;

    BufferViewLocation *loc = BufferViewLocation_GetEntry(view);
    if(loc == nullptr){
        if(globalTakenPositions < MAX_BUFFERVIEWS){
            loc = &globalLocations[globalTakenPositions++];
            loc->view = view;
        }else{
            return;
        }
    }

    BufferView_GetCursor(view, &cursor);
    location.textPosition  = cursor->textPosition;
    location.ghostPosition = cursor->ghostPosition;
    location.range = BufferView_GetViewRange(view);
    loc->locationMap[lineBuffer] = location;
}
