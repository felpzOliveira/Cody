#include <buffers.h>
#include <geometry.h>
#include <time.h>
#include <utilities.h>
#include <app.h>
#include <string.h>
#include <autocomplete.h>
#include <parallel.h>
#include <storage.h>
#include <sstream>
#include <log.h>

#define MODULE_NAME "Buffer"

inline void DuplicateToken(Token *dst, Token *src){
    if(dst == nullptr){
        printf("Null token given\n");
        return;
    }

    dst->size = src->size;
    dst->position = src->position;
    dst->identifier = src->identifier;
    if(dst->reserved){
        AllocatorFree(dst->reserved);
        dst->reserved = nullptr;
    }

    if(src->reserved){
        dst->reserved = StringDup((char *)src->reserved, src->size);
    }
}

inline void CopyToken(Token *dst, Token *src){
    dst->size = src->size;
    dst->position = src->position;
    dst->identifier = src->identifier;
    if(dst->reserved){
        AllocatorFree(dst->reserved);
        dst->reserved = nullptr;
    }
    dst->reserved = src->reserved;
}

void Buffer_SoftClear(Buffer *buffer){
    Buffer_RemoveRangeRaw(buffer,  0, buffer->taken);
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->reserved){
            AllocatorFree(token->reserved);
            token->reserved = nullptr;
        }
    }

    if(buffer->tokens){
        AllocatorFree(buffer->tokens);
        buffer->tokens = nullptr;
    }

    buffer->tokenCount = 0;
}

void Buffer_RemoveExcessSpace(Buffer *buffer){
    uint tid = 0;
    Token *target = nullptr;
    return;

    if(!buffer) return;
    if(!buffer->is_ours) return;
    // avoid removing space when only 1 token is present
    // mostly for preserving the indent region offset auto-generated
    if(!(buffer->tokenCount > 1)) return;

    for(int i = (int)buffer->tokenCount - 1; i >= 0; i--){
        Token *token = &buffer->tokens[i];
        if(token->identifier == TOKEN_ID_SPACE){
            target = token;
            tid = (uint)i;
        }else{
            break;
        }
    }

    if(target){
        for(uint i = target->position; i < buffer->size; i++){
            buffer->data[i] = 0;
        }
        buffer->taken = target->position;
        buffer->tokenCount = tid;
        buffer->count = Buffer_GetUtf8Count(buffer);
    }
}

void Buffer_CopyReferences(Buffer *dst, Buffer *src){
    if(dst != nullptr && src != nullptr){
        dst->data = src->data;
        dst->count = src->count;
        dst->taken = src->taken;
        dst->size = src->size;
        dst->tokenCount = src->tokenCount;
        dst->tokens = src->tokens;
        dst->stateContext = src->stateContext;
        dst->is_ours = src->is_ours;
        dst->erased = src->erased;
    }
}

void Buffer_CopyDeep(Buffer *dst, Buffer *src){
    if(dst != nullptr && src != nullptr){
        if(dst->data == nullptr){
            uint len = Max(src->size, DefaultAllocatorSize);
            dst->data = (char *)AllocatorGet(len * sizeof(char));
            dst->size = len;
        }

        if(src->taken > dst->size){
            dst->data = AllocatorExpand(char, dst->data, src->size, dst->size);
            dst->size = src->size;
        }

        if(dst->tokens == nullptr && src->tokenCount > 0){
            dst->tokens = (Token *)AllocatorGet(src->tokenCount * sizeof(Token));
        }else if(dst->tokenCount < src->tokenCount){
            if(dst->tokens == nullptr){
                dst->tokens = AllocatorGetN(Token, src->tokenCount);
            }else{
                dst->tokens = AllocatorExpand(Token, dst->tokens, src->tokenCount,
                                              dst->tokenCount);
            }
        }

        if(src->taken > 0){
            Memcpy(dst->data, src->data, src->taken);
        }

        if(src->tokenCount > 0){
            for(uint i = 0; i < src->tokenCount; i++){
                Token *srcToken = &src->tokens[i];
                Token *dstToken = &dst->tokens[i];
                if(!(i < dst->tokenCount)){
                    // If dstToken was outside of the scope of the dst buffer
                    // it could happen that it contains garbage in its memory
                    // because its data was not dependent on its definition.
                    // So we manually need to force its reserved field to nullptr
                    // to avoid the duplication routine to mistakenly attempt to free
                    // its content.
                    dstToken->reserved = nullptr;
                }

                DuplicateToken(dstToken, srcToken);
            }
        }

        dst->erased = src->erased;
        dst->is_ours = src->is_ours;
        dst->count = src->count;
        dst->taken = src->taken;
        dst->tokenCount = src->tokenCount;
        dst->stateContext = src->stateContext;
    }
}

int Buffer_FindFirstNonEmptyToken(Buffer *buffer){
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier != TOKEN_ID_SPACE){
            return (int)i;
        }
    }

    return -1;
}

uint Buffer_FindFirstNonEmptyLocation(Buffer *buffer){
    if(buffer == nullptr) return 0;

    int index = Buffer_FindFirstNonEmptyToken(buffer);
    if(index < 0) return 0;

    return buffer->tokens[index].position;
}

uint Buffer_GetTokenAt(Buffer *buffer, uint u8){
    uint pos = Buffer_Utf8PositionToRawPosition(buffer, u8);
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
		uint p = token->position < 0 ? 0 : token->position;
		uint size = token->size < 0 ? 0 : token->size;
        if(p <= pos && size + p > pos){
            return i;
        }
    }

    return buffer->tokenCount;
}

uint Buffer_PositionTabCompensation(Buffer *buffer, uint rawp, int direction){
    if(buffer->taken == 0) return 0;

    if(!(rawp <= buffer->taken)){
        BUG();
        Buffer_DebugStdoutData(buffer);
        BUG_PRINT("Queried for tab compensation at %u\n", rawp);
        return 0;
    }

    direction = direction < 0 ? -1 : 1;
    while(buffer->data[rawp] == '\t'){
        if(direction < 0){
            if(rawp < 1) break;
        }else{
            if(rawp >= buffer->taken-1){
                // NOTE: The end of the buffer is a tab
                // we need to return the maximum possible position
                // otherwise whenever we insert a tab in an empty line
                // we would return tabLen-1 which is the position of a tab
                // and not an actual writable position.
                return buffer->taken;
            }
        }

        rawp += direction;
    }

    return rawp;
}

uint Buffer_Utf8RawPositionToPosition(Buffer *buffer, uint rawp){
    if(!(rawp <= buffer->taken)){
        BUG();
        BUG_PRINT("BUG tracked:\n");
        if(buffer->data == nullptr){
            BUG_PRINT("Buffer is empty (nullptr), recovering\n");
            // attempt a recover
            Buffer_Init(buffer, DefaultAllocatorSize);
            return 0;
        }
        Buffer_DebugStdoutData(buffer);
        BUG_PRINT("Queried for UTF-8 location at %u\n", rawp);
        rawp = buffer->taken;
    }

    uint r = 0;
    if(buffer->taken > 0){
        char *p = buffer->data;
        int c = 0;
        int of = 0;
        while(c != (int)rawp && (int)buffer->taken > c){
            of = 0;
            int rv = StringToCodepoint(&p[c], buffer->taken - c, &of);
            if(rv == -1) break;
            c += of;
            r++;
        }

        if(c != (int)rawp){
            BUG();
            BUG_PRINT("Failed UTF-8 decode at %u for the following buffer:\n"
                      "***************************\n", rawp);
            Buffer_DebugStdoutData(buffer);
        }
    }

    return r;
}

void Buffer_EraseSymbols(Buffer *buffer, SymbolTable *symTable){
    if(buffer){
        for(uint i = 0; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            if(Symbol_IsTokenAutoCompletable(token->identifier) &&
               token->size > AutoCompleteMinInsertLen)
            {
                char *p = &buffer->data[token->position];
                AutoComplete_Remove(p, token->size);

                if(Lex_IsUserToken(token) && symTable){
                    if(token->reserved != nullptr){
                        char *label = (char *)token->reserved;
                        SymbolTable_Remove(symTable, label, token->size, token->identifier);
                        AllocatorFree(token->reserved);
                        token->reserved = nullptr;
                    }
                }
            }
        }

        buffer->erased = true;
    }
}

//TODO: Review, this might be showing some issues with lexer
uint Buffer_Utf8PositionToRawPosition(Buffer *buffer, uint u8p, int *len){
    if(!(u8p <= buffer->taken)){
        BUG();
        BUG_PRINT("BUG tracked:\n");
        if(buffer->data == nullptr){
            BUG_PRINT("Buffer is empty (nullptr)\n");
        }
        Buffer_DebugStdoutData(buffer);
        BUG_PRINT("Queried for UTF-8 translation at %u\n", u8p);
        u8p = buffer->count;
    }

    uint r = 0;
    if(buffer->taken > 0){
        r = StringComputeRawPosition(buffer->data, buffer->taken, u8p, len);
    }else{
        if(len) *len = 1;
    }

    return r;
}

void Buffer_Claim(Buffer *buffer){
    if(buffer){
        buffer->is_ours = true;
    }
}

uint Buffer_GetUtf8Count(Buffer *buffer){
    return StringComputeU8Count(buffer->data, buffer->taken);
}

void Buffer_RemoveLastToken(Buffer *buffer){
    if(buffer){
        if(buffer->tokenCount < 1) return;
        Token *token = &buffer->tokens[buffer->tokenCount-1];
        Buffer_RemoveRangeRaw(buffer, token->position, buffer->taken);
        if(token->reserved) AllocatorFree(token->reserved);
        if(buffer->tokenCount > 1){
            buffer->tokens = AllocatorExpand(Token, buffer->tokens,
                                       buffer->tokenCount-1, buffer->tokenCount);
        }else{
            AllocatorFree(buffer->tokens);
            buffer->tokens = nullptr;
        }
        buffer->tokenCount -= 1;
    }
}

void Buffer_UpdateTokens(Buffer *buffer, Token *tokens, uint size){
    if(buffer){
        bool release = true;
        if(buffer->tokenCount < size){
            if(buffer->tokens){
                buffer->tokens = AllocatorExpand(Token, buffer->tokens,
                                                 size, buffer->tokenCount);
                for(uint i = buffer->tokenCount; i < size; i++){
                    buffer->tokens[i].reserved = nullptr;
                }

            }else{
                buffer->tokens = AllocatorGetN(Token, size);
                buffer->tokenCount = size;
                release = false;
            }
        }

        if(release){
            for(uint i = 0; i < buffer->tokenCount; i++){
                Token *token = &buffer->tokens[i];
                if(token->reserved != nullptr) AllocatorFree(token->reserved);
                token->reserved = nullptr;
            }
        }

        if(size == 0){
            AllocatorFree(buffer->tokens);
            buffer->tokens = nullptr;
            buffer->tokenCount = 0;
        }else{
            if(size < buffer->tokenCount){
                buffer->tokens = AllocatorExpand(Token, buffer->tokens,
                                                 size, buffer->tokenCount);
            }

            buffer->tokenCount = size;
            for(uint i = 0; i < size; i++){
                Token *dstToken = &buffer->tokens[i];
                Token *srcToken = &tokens[i];
                CopyToken(dstToken, srcToken);
                srcToken->reserved = nullptr;
            }
        }
    }
}

void Buffer_Init(Buffer *buffer, uint size){
    AssertA(buffer != nullptr && size > 0, "Invalid buffer initialization");
    buffer->data = (char *)AllocatorGet(size * sizeof(char));
    buffer->size = size;
    buffer->count = 0;
    buffer->taken = 0;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
    buffer->stateContext.state = TOKENIZER_STATE_CLEAN;
    buffer->stateContext.activeWorkProcessor = -1;
    buffer->stateContext.backTrack = 0;
    buffer->stateContext.forwardTrack = 0;
    buffer->is_ours = false;
}

void Buffer_InitSet(Buffer *buffer, char *head, uint leno){
    uint len = leno;

    if(buffer->data == nullptr){
        buffer->size = len+DefaultAllocatorSize;
        buffer->data = (char *)AllocatorGet((buffer->size) * sizeof(char));
    }else{
        if(buffer->size < len){
            uint newSize = buffer->size + len + DefaultAllocatorSize;
            buffer->data = AllocatorExpand(char, buffer->data, newSize, buffer->size);
            buffer->size = newSize;
        }
    }

    uint ic = 0;
    if(len > 0){
        for(uint i = 0; i < leno; i++){
            if(head[i] != '\n'){
                buffer->data[ic++] = head[i];
            }
        }
    }
    buffer->taken = ic;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
    buffer->stateContext.state = TOKENIZER_STATE_CLEAN;
    buffer->stateContext.activeWorkProcessor = -1;
    buffer->stateContext.backTrack = 0;
    buffer->stateContext.forwardTrack = 0;
    buffer->count = Buffer_GetUtf8Count(buffer);
    buffer->is_ours = false;
}

int Buffer_IsBlank(Buffer *buffer){
    if(buffer->taken == 0) return 1;
    for(uint i = 0; i < buffer->taken; i++){
        char p = buffer->data[i];
        if(p != ' ' && p != '\r' && p != '\t' && p != '\n') return 0;
    }

    return 1;
}

uint Buffer_FindPreviousSeparator(Buffer *buffer, uint rawp){
    if(buffer->taken == 0 || rawp == 0) return 0;
    if(rawp > buffer->taken){
        BUG();
        BUG_PRINT("BUG tracked: Attempted to compute previous location for a"
                  " position outside buffer (%u > %u)\n", rawp, buffer->taken);
        return 0;
    }

    uint n = rawp-1;
    // skip empty chars
    while(TerminatorChar(buffer->data[n]) && n > 0) n--;

    for(; n > 0; n--){
        if(TerminatorChar(buffer->data[n])){ // TODO: use TerminatorChar?
            return n+1;
        }
    }
    return n;
}

uint Buffer_FindNextSeparator(Buffer *buffer, uint rawp){
    if(buffer->taken == 0) return 0;
    if(rawp > buffer->taken){
        BUG();
        BUG_PRINT("BUG tracked: Attempted to compute next location for a"
                  " position outside buffer (%u > %u)\n", rawp, buffer->taken);
        return 0;
    }

    uint n = rawp+1;
    // skip empty chars
    while(TerminatorChar(buffer->data[n]) && n < buffer->taken) n++;

    for(; n < buffer->taken; n++){
        if(TerminatorChar(buffer->data[n])){ // TODO: use TerminatorChar?
            return n;
        }
    }
    return n;
}

uint Buffer_FindPreviousWordU8(Buffer *buffer, uint u8, const char *term, uint len){
    uint raw = Buffer_Utf8PositionToRawPosition(buffer, u8);
    if(raw > buffer->taken){
        BUG();
        BUG_PRINT("BUG tracked: Attempted to query buffer outside of valid range (%u > %u)\n",
                  raw, buffer->taken);
        return u8;
    }

    if(raw == 0) return u8;
    uint res = 0;
    bool done = false;
    for(int i = (int)raw-1; i >= 0 && !done; i--){
        for(uint k = 0; k < len && !done; k++){
            if(buffer->data[i] == term[k]){
                res = Buffer_Utf8RawPositionToPosition(buffer, i);
                done = true;
                break;
            }
        }
    }
    return res;
}

uint Buffer_FindNextWord8(Buffer *buffer, uint u8, const char *term, uint len){
    uint raw = Buffer_Utf8PositionToRawPosition(buffer, u8);
    if(raw > buffer->taken){
        BUG();
        BUG_PRINT("BUG tracked: Attempted to query buffer outside of valid range (%u > %u)\n",
                raw, buffer->taken);
        return u8;
    }

    if(raw == buffer->taken) return u8;
    uint res = buffer->taken;
    bool done = false;
    for(uint i = raw+1; i < buffer->taken && !done; i++){
        for(uint k = 0; k < len && !done; k++){
            if(buffer->data[i] == term[k]){
                res = i;
                done = true;
                break;
            }
        }
    }
    return Buffer_Utf8RawPositionToPosition(buffer, res);
}

void Buffer_RemoveRangeRaw(Buffer *buffer, uint start, uint end){
    if(end > start){
        uint endLoc = Min(buffer->taken, end);
        uint rangeLen = endLoc - start;
        if(buffer->taken > rangeLen){
            for(uint i = endLoc, j = 0; i < buffer->taken; i++, j++){
                buffer->data[start+j] = buffer->data[i];
            }

            buffer->taken -= rangeLen;
            buffer->count = Buffer_GetUtf8Count(buffer);
        }else{
            buffer->taken = 0;
            buffer->count = 0;
        }

        buffer->data[buffer->taken] = 0;
    }
}

void Buffer_RemoveRange(Buffer *buffer, uint u8start, uint u8end){
    if(u8end > u8start){
        uint start = Buffer_Utf8PositionToRawPosition(buffer, u8start, nullptr);
        uint end = Buffer_Utf8PositionToRawPosition(buffer, u8end, nullptr);
        uint endLoc = Min(buffer->taken, end);
        uint rangeLen = endLoc - start;
        if(buffer->taken > rangeLen){
            for(uint i = endLoc, j = 0; i < buffer->taken; i++, j++){
                buffer->data[start+j] = buffer->data[i];
            }

            buffer->taken -= rangeLen;
            buffer->count = Buffer_GetUtf8Count(buffer);
        }else{
            buffer->taken = 0;
            buffer->count = 0;
        }

        buffer->data[buffer->taken] = 0;
    }
}

uint Buffer_InsertRawStringAt(Buffer *buffer, uint at, char *str, uint len){
    uint ic = 0;
    uint osize = len;
    if(len > 0){
        if(buffer->data == nullptr){
            uint newSize = at + len + DefaultAllocatorSize;
            buffer->data = AllocatorGetN(char, newSize);
            buffer->size = newSize;
            buffer->taken = 0;
        }

        if(buffer->size <= buffer->taken + len){
            uint newSize = buffer->size + len + DefaultAllocatorSize;
            buffer->data = AllocatorExpand(char, buffer->data, newSize, buffer->size);
            buffer->size = newSize;
        }

        if(buffer->taken > at){
            uint shiftSize = buffer->taken - at + 1;
            for(uint i = 0; i < shiftSize; i++){
                buffer->data[buffer->taken-i+len] = buffer->data[buffer->taken-i];
            }
        }

        for(uint i = 0; i < osize; i++){
            buffer->data[at+ic] = str[i];
            ic++;
        }

        buffer->taken += len;
        buffer->count = Buffer_GetUtf8Count(buffer);
        buffer->data[buffer->taken] = 0;

        if(buffer->size > buffer->taken){
            // ??? buffer->data[buffer->taken] = 0;
        }

    }else if(buffer->size == 0 || buffer->data == nullptr){
        Buffer_Init(buffer, DefaultAllocatorSize);
    }

    AssertA(buffer->size >= buffer->taken, "Out of bounds write");
    return ic;
}

uint Buffer_InsertStringAt(Buffer *buffer, uint rap, char *str, uint len){
    uint at = Buffer_Utf8PositionToRawPosition(buffer, rap);
    return Buffer_InsertRawStringAt(buffer, at, str, len);
}

void Buffer_Release(Buffer *buffer){
    buffer->data = nullptr;
    buffer->size = 0;
    buffer->count = 0;
    buffer->taken = 0;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
    buffer->stateContext.state = TOKENIZER_STATE_CLEAN;
    buffer->stateContext.activeWorkProcessor = -1;
    buffer->stateContext.backTrack = 0;
    buffer->stateContext.forwardTrack = 0;
}

void Buffer_FastTokenGen(Buffer *buffer){
    std::vector<Token> tokens;
    int start = -1;
    int end = -1;
    for(uint k = 0; k < buffer->taken; k++){
        char p = buffer->data[k];
        if(p != ' ' && p != '\r' && p != '\t' && p != '\n'){
            if(end >= 0){
                tokens.push_back({
                    .size = (int)k - end,
                    .position = (int)k,
                    .identifier = TOKEN_ID_SPACE,
                    .reserved = nullptr,
                });
                end = -1;
            }

            if(start < 0){
                start = (int)k;
            }
        }else{
            if(start >= 0){
                end = (int)k;
                tokens.push_back({
                    .size = end - start,
                    .position = start,
                    .identifier = TOKEN_ID_NONE,
                    .reserved = nullptr,
                });
                start = -1;
            }
        }
    }

    if(start >= 0){
        end = (int)buffer->taken;
        tokens.push_back({
            .size = end - start,
            .position = start,
            .identifier = TOKEN_ID_NONE,
            .reserved = nullptr,
        });
        start = -1;
    }

    Buffer_UpdateTokens(buffer, tokens.data(), tokens.size());
    tokens.clear();
}

void Buffer_Free(Buffer *buffer){
    if(buffer){
#if defined(MEMORY_DEBUG)
        if((buffer->data && buffer->size == 0)){
            printf("[MEM] Buffer has data pointer but 0 size");
            getchar();
        }
#endif
        if(buffer->data && buffer->size > 0) AllocatorFree(buffer->data);
        if(buffer->tokens && buffer->tokenCount > 0){
            for(uint i = 0; i < buffer->tokenCount; i++){
                Token *token = &buffer->tokens[i];
                if(token->reserved){
                    AllocatorFree(token->reserved);
                    token->reserved = nullptr;
                }
            }
            AllocatorFree(buffer->tokens);
        }
        Buffer_Release(buffer);
    }
}

uint LineBuffer_InsertRawText(LineBuffer *lineBuffer, char *text, uint size){
    AssertA(lineBuffer != nullptr, "Invalid line initialization");
    if(lineBuffer->lineCount == 0){
        printf("Empty lineBuffer given\n"); // should never happen
        return 0;
    }
    uint lastLine = lineBuffer->lineCount - 1;
    Buffer *buffer = lineBuffer->lines[lastLine];
    uint u8offset = buffer->count;
    uint dummy = 0;

    return LineBuffer_InsertRawTextAt(lineBuffer, text, size, lastLine,
                                      u8offset, &dummy, 1);
}

void LineBuffer_InsertLine(LineBuffer *lineBuffer, char *line, uint size){
    AssertA(lineBuffer != nullptr, "Invalid line initialization");
    if(!(lineBuffer->lineCount < lineBuffer->size)){
        uint newSize = lineBuffer->size + DefaultAllocatorSize;
        lineBuffer->lines = AllocatorExpand(Buffer *, lineBuffer->lines, newSize,
                                            lineBuffer->size);

        for(uint i = 0; i < DefaultAllocatorSize; i++){
            lineBuffer->lines[lineBuffer->size+i] = (Buffer *)AllocatorGet(sizeof(Buffer));
            *(lineBuffer->lines[lineBuffer->size+i]) = BUFFER_INITIALIZER;
        }

        lineBuffer->size = newSize;
    }
    if(size > 0 && line)
        Buffer_InitSet(lineBuffer->lines[lineBuffer->lineCount], line, size);
    else
        Buffer_Init(lineBuffer->lines[lineBuffer->lineCount], DefaultAllocatorSize);
    lineBuffer->lineCount++;
}

void LineBuffer_SoftClear(LineBuffer *lineBuffer){
    for(uint i = 0; i < lineBuffer->lineCount; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        Buffer_SoftClear(buffer);
        //Buffer_RemoveRangeRaw(buffer, 0, buffer->taken);
    }
    // need to give the empty line address
    lineBuffer->lineCount = 1;
}

void LineBuffer_RemoveLineAt(LineBuffer *lineBuffer, uint at){
    if(lineBuffer->lineCount > at){
        // swap pointers forward from 'at'
        Buffer *Li = lineBuffer->lines[at];
        Buffer_Free(Li);
        for(uint i = at; i < lineBuffer->lineCount-1; i++){
            // Line N receives the pointer from the next line N+1
            Buffer *lineN   = lineBuffer->lines[i];
            Buffer *lineNp1 = lineBuffer->lines[i+1];
            Buffer_CopyReferences(lineN, lineNp1);
        }

        // we need to release the pointers from the final line as it now
        // belongs to line located at 'lineCount-1'
        Buffer_Release(lineBuffer->lines[lineBuffer->lineCount-1]);
        lineBuffer->lineCount--;
    }
}

void LineBuffer_MergeConsecutiveLines(LineBuffer *lineBuffer, uint base){
    if(base+1 < lineBuffer->lineCount){
        Buffer *Li = LineBuffer_GetBufferAt(lineBuffer, base);
        Buffer *Li1 = LineBuffer_GetBufferAt(lineBuffer, base+1);
        Buffer_InsertStringAt(Li, Li->count, Li1->data, Li1->taken);
        LineBuffer_RemoveLineAt(lineBuffer, base+1);
    }
}

void LineBuffer_InsertLineAt(LineBuffer *lineBuffer, uint at, char *line, uint size){
    Buffer *Li = nullptr;
    // make sure we can hold at least one more line
    if(!(lineBuffer->lineCount < lineBuffer->size)){
        uint newSize = lineBuffer->size + DefaultAllocatorSize;
        lineBuffer->lines = AllocatorExpand(Buffer *, lineBuffer->lines,
                                            newSize, lineBuffer->size);

        for(uint i = 0; i < DefaultAllocatorSize; i++){
            lineBuffer->lines[lineBuffer->size+i] = (Buffer *)AllocatorGet(sizeof(Buffer));
            *(lineBuffer->lines[lineBuffer->size+i]) = BUFFER_INITIALIZER;
        }

        lineBuffer->size = newSize;
    }

    if(lineBuffer->lineCount <= at){ // insert at the end of the linebuffer
        Li = lineBuffer->lines[lineBuffer->lineCount];
    }else{
        // swap pointers from the end until reach 'at'
        for(uint i = lineBuffer->lineCount; i > at; i--){
            // Line N receives the pointer from the previous line N-1
            Buffer *lineN   = lineBuffer->lines[i];
            Buffer *lineNm1 = lineBuffer->lines[i-1];
            Buffer_CopyReferences(lineN, lineNm1);
        }

        // once we reach the line 'at' we create a new line at this location
        Li = lineBuffer->lines[at];
        // we need to allocate a new buffer for this line as its pointer now belongs
        // to the line 'at+1', reset its data configuration
        Li->data = nullptr;
        Li->size = 0;
        Li->count = 0;
    }

    if(line && size > 0)
        Buffer_InitSet(Li, line, size);
    else
        Buffer_Init(Li, DefaultAllocatorSize);
    lineBuffer->lineCount++;
}

void LineBuffer_InitBlank(LineBuffer *lineBuffer){
    AssertA(lineBuffer != nullptr, "Invalid line buffer blank initialization");
    lineBuffer->lines = AllocatorGetDefault(Buffer *);
    lineBuffer->lineCount = 0;
    lineBuffer->is_dirty = 0;
    lineBuffer->size = DefaultAllocatorSize;
    lineBuffer->undoRedo.undoStack = DoStack_Create();
    lineBuffer->undoRedo.redoStack = DoStack_Create();
    lineBuffer->activeBuffer = vec2i(-1, -1);
    lineBuffer->props.cpSection = {
        .start = vec2ui(),
        .end = vec2ui(),
        .currTime = 0,
        .interval = 0,
        .active = 0,
    };

    lineBuffer->props.isWrittable = true;
    lineBuffer->props.isInternal = false;

    for(uint i = 0; i < DefaultAllocatorSize; i++){
        lineBuffer->lines[i] = (Buffer *)AllocatorGet(sizeof(Buffer));
        *(lineBuffer->lines[i]) = BUFFER_INITIALIZER;
    }

}

struct LineBufferTokenizer{
    Tokenizer *tokenizer;
    LineBuffer *lineBuffer;
    int lineBacktrack;
    std::function<void(int)> func;
};


static LineBuffer *activeLineBuffer = nullptr;
static uint current = 0;
static uint totalSize = 0;
static char *content = nullptr;


uint LineBuffer_TokenizerFileFetcher(char **p, uint fet){
    if(content && totalSize > fet + current){
        *p = &content[fet + current];
        return totalSize - (fet + current);
    }

    *p = nullptr;
    return 0;
}

#define DEBUG_TOKENS  0
static void LineBuffer_LineProcessor(char **p, uint size, uint lineNr,
                                     uint at, uint total, void *prv)
{
    LineBufferTokenizer *lineBufferTokenizer = (LineBufferTokenizer *)prv;
    Tokenizer *tokenizer = lineBufferTokenizer->tokenizer;
    LineBuffer *lineBuffer = lineBufferTokenizer->lineBuffer;
    TokenizerWorkContext *workContext = tokenizer->workContext;

    TokenizerStateContext tokenizerContext;
    Lex_TokenizerGetCurrentState(tokenizer, &tokenizerContext);

    LineBuffer_InsertLine(lineBuffer, *p, size-1);
    workContext->workTokenListHead = 0;

    Lex_TokenizerPrepareForNewLine(tokenizer, lineNr-1);
#if DEBUG_TOKENS != 0
    char *s = *p;
    printf("==============================================\n");
    printf("{ %d }: %s\n", (int)lineNr, *p);
    printf("==============================================\n");
#endif
    int iSize = size-1;
    current = at;
    lineBufferTokenizer->func(lineNr);

    do{
        Token token;
        token.reserved = nullptr;
        char *h = *p;
        int rc = Lex_TokenizeNext(p, iSize, &token, tokenizer, 0);
        if(rc < 0){
            iSize = 0;
        }else{
            uint head   = workContext->workTokenListHead;
            uint length = workContext->workTokenListSize;
            if(head >= length){
                uint inc = 32;
                workContext->workTokenList = AllocatorExpand(Token,
                                                             workContext->workTokenList,
                                                             length+inc, length);
                length += inc;
                workContext->workTokenListSize = length;
            }

            workContext->workTokenList[head].size = token.size;
            workContext->workTokenList[head].position = token.position;
            workContext->workTokenList[head].identifier = token.identifier;
            workContext->workTokenList[head].reserved = token.reserved;
            workContext->workTokenListHead++;

            if(Symbol_IsTokenAutoCompletable(token.identifier) &&
               token.size > AutoCompleteMinInsertLen)
            {
                AutoComplete_PushString(h, token.size);
            }
#if DEBUG_TOKENS != 0
            char *h = &s[token.position];
            char f = s[token.position+token.size];
            s[token.position+token.size] = 0;
            printf("Token: %s { %s } [%d]\n", h,
                   Symbol_GetIdString(token.identifier), token.position);
            s[token.position+token.size] = f;
#endif
            iSize -= rc;
            current += rc;
        }
    }while(iSize > 0 && **p != 0);

    LineBuffer_CopyLineTokens(lineBuffer, lineNr-1, workContext->workTokenList,
                              workContext->workTokenListHead);

    if(Lex_TokenizerHasPendingWork(tokenizer)){
        uint r = tokenizerContext.backTrack;
        AssertA(lineNr-1 >= r, "Overflow during forwardtrack computation");
        Buffer *b = LineBuffer_GetBufferAt(lineBuffer, lineNr - 1 - r);
        b->stateContext.forwardTrack = r+2;
    }

    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, lineNr-1);
    buffer->stateContext = tokenizerContext;
    buffer->stateContext.forwardTrack = 0;
    buffer->erased = false;

}

static uint currentID;
uint LineBuffer_BufferFetcher(char **p, uint fet){
    Buffer *b = LineBuffer_GetBufferAt(activeLineBuffer, currentID+1);
    currentID++;
    if(b){
        *p = b->data;
        return b->taken;
    }

    *p = NULL;
    return 0;
}

static void LineBuffer_RemountBuffer(LineBuffer *lineBuffer, Buffer *buffer,
                                     Tokenizer *tokenizer, uint base)
{
    TokenizerStateContext tokenizerContext;
    Lex_TokenizerGetCurrentState(tokenizer, &tokenizerContext);

    buffer->stateContext = tokenizerContext;
    buffer->stateContext.forwardTrack = 0;

    TokenizerWorkContext *workContext = tokenizer->workContext;

    workContext->workTokenListHead = 0;

    char *p = buffer->data;
    char *h = p;
    int totalSize = buffer->taken;

    int size = buffer->taken;
    Lex_TokenizerPrepareForNewLine(tokenizer, base);

    do{
        Token token;
        token.reserved = nullptr;
        int rc = Lex_TokenizeNext(&p, size, &token, tokenizer, 1);
        if(rc < 0){
            size = 0;
        }else{
            uint head   = workContext->workTokenListHead;
            uint length = workContext->workTokenListSize;
            if(head >= length){
                uint inc = 32;
                workContext->workTokenList = AllocatorExpand(Token,
                                                             workContext->workTokenList,
                                                             length + inc, length);
                length += inc;
                workContext->workTokenListSize = length;
            }

            workContext->workTokenList[head].size = token.size;
            workContext->workTokenList[head].position = token.position;
            workContext->workTokenList[head].identifier = token.identifier;
            workContext->workTokenList[head].reserved = token.reserved;
            workContext->workTokenListHead++;

            if(Symbol_IsTokenAutoCompletable(token.identifier) &&
               token.size > AutoCompleteMinInsertLen)
            {
                AutoComplete_PushString(&h[token.position], token.size);
            }

            size = totalSize - token.position - token.size;
        }
    }while(size > 0 && *p != 0);

    Buffer_UpdateTokens(buffer, workContext->workTokenList,
                        workContext->workTokenListHead);

    if(Lex_TokenizerHasPendingWork(tokenizer)){
        uint r = tokenizerContext.backTrack;
        AssertA(base >= r, "Overflow during forwardtrack computation");
        Buffer *b = LineBuffer_GetBufferAt(lineBuffer, base - r);
        b->stateContext.forwardTrack = r+2;
    }
}

std::vector<vec2ui> *LineBuffer_GetDiffRangePtr(LineBuffer *lineBuffer, bool *any){
    std::vector<vec2ui> *ptr = nullptr;
    *any = false;
    if(lineBuffer){
        ptr = &lineBuffer->props.diffLines;
        *any = ptr->size() > 0;
    }

    return ptr;
}


std::vector<LineHighlightInfo> *LineBuffer_GetLineHighlightPtr(LineBuffer *lineBuffer, int clear){
    std::vector<LineHighlightInfo> *ptr = nullptr;
    if(lineBuffer){
        ptr = &lineBuffer->props.diffs;
        if(clear){
            ptr->clear();
        }
    }
    return ptr;
}

void LineBuffer_EraseDiffContent(LineBuffer *lineBuffer){
    if(lineBuffer){
        std::vector<vec2ui> *ptr = &lineBuffer->props.diffLines;
        for(int i = ptr->size()-1; i >= 0; i--){
            vec2ui v = ptr->at(i);
            if(v.y == GIT_LINE_REMOVED){
                LineBuffer_RemoveLineAt(lineBuffer, v.x);
            }
        }
    }
}

void LineBuffer_InsertDiffContent(LineBuffer *lineBuffer, vec2ui &range){
    if(lineBuffer){
        std::vector<vec2ui> *ptr = &lineBuffer->props.diffLines;
        std::vector<LineHighlightInfo> *dif = &lineBuffer->props.diffs;
        ptr->clear();
        /*
        * Line computation: Whenever we look at a delta we need to keep
        * in mind that for lines that were removed their number are relating
        * to the old file, while for new lines they are referred to the new file.
        * In the editor, new lines are always correct because the file is already
        * completed. If we wish to merge both views in the same file we need to
        * concatenate the removed lines in the correct position. Every time we
        * insert a removed line, we push all the file down so even for the new lines
        * they get position n += 1. For the following removed lines we need to consider
        * the new lines that entered. If a removed line was at position k (old file)
        * but before it were s new lines, than in the new file the position is actually
        * k+s. These direct translates into the following loop where we accumulate
        * the amount of removed lines to push the new lines down, and accumulate the
        * new lines in order to push the removed lines based on the deltas that entered.
        */

        uint start = lineBuffer->lineCount, end = 0;
        uint adds = 0, rems = 0;
        for(uint i = 0; i < dif->size(); i++){
            LineHighlightInfo line = dif->at(i);
            uint n = line.lineno-1;

            //std::string p("+");
            if(line.ctype == GIT_LINE_REMOVED){
                n += rems;
                LineBuffer_InsertLineAt(lineBuffer, n, (char *)line.content.c_str(),
                                        line.content.size());
                adds += 1;
                //p = "-";
            }else if(line.ctype == GIT_LINE_INSERTED){
                n += adds;
                rems += 1;
            }else{
                AssertA(0, "Invalid delta type");
            }

            //printf("[%d - %d] => %s %s\n", (int)n, (int)line.lineno-1,
                    //p.c_str(), line.content.c_str());

            start = Min(start, n);
            end = Max(end, n);

            ptr->push_back(vec2ui(n, (uint)line.ctype));
        }

        range = vec2ui(start, end);
    }
}

void LineBuffer_FastTokenGen(LineBuffer *lineBuffer, uint base, uint offset){
    Buffer *buffer = nullptr;
    uint i = base;
    uint expectedEnd = base + offset + 1;

    std::vector<Token> tokens; // slow?

    while(i < expectedEnd){
        buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        int start = -1;
        int end = -1;
        for(uint k = 0; k < buffer->taken; k++){
            char p = buffer->data[k];
            if(p != ' ' && p != '\r' && p != '\t' && p != '\n'){
                if(end >= 0){
                    tokens.push_back({
                        .size = (int)k - end,
                        .position = (int)k,
                        .identifier = TOKEN_ID_SPACE,
                        .reserved = nullptr,
                    });
                    end = -1;
                }

                if(start < 0){
                    start = (int)k;
                }
            }else{
                if(start >= 0){
                    end = (int)k;
                    tokens.push_back({
                        .size = end - start,
                        .position = start,
                        .identifier = TOKEN_ID_NONE,
                        .reserved = nullptr,
                    });
                    start = -1;
                }
            }
        }

        if(start >= 0){
            end = (int)buffer->taken;
            tokens.push_back({
                .size = end - start,
                .position = start,
                .identifier = TOKEN_ID_NONE,
                .reserved = nullptr,
            });
            start = -1;
        }

        Buffer_UpdateTokens(buffer, tokens.data(), tokens.size());
        tokens.clear();
        i++;
    }
}

void LineBuffer_ReTokenizeFromBuffer(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                                     uint base, uint offset)
{
    uint i = 0;
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, base);
    TokenizerStateContext *stateContext = &buffer->stateContext;
    SymbolTable *symTable = tokenizer->symbolTable;
    uint start = base - stateContext->backTrack;
    AssertA(start < lineBuffer->lineCount, "BUG: Overflow during backtrack computation");

    buffer = LineBuffer_GetBufferAt(lineBuffer, start);
    uint expectedEnd = start + buffer->stateContext.forwardTrack + offset + 1;

    Lex_TokenizerRestoreFromContext(tokenizer, &buffer->stateContext);
    Lex_TokenizerSetFetchCallback(tokenizer, LineBuffer_BufferFetcher);

    i = start;
    activeLineBuffer = lineBuffer;
    while((i < expectedEnd || Lex_TokenizerHasPendingWork(tokenizer))){
        currentID = i;
        buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        // Before re-tokenizing check for user tokens and allow symbol table
        // to remove them
        if(!buffer->erased){
            for(uint s = 0; s < buffer->tokenCount; s++){
                Token *token = &buffer->tokens[s];
                if(token){
                    if(Symbol_IsTokenAutoCompletable(token->identifier) &&
                       token->size > AutoCompleteMinInsertLen)
                    {
                        char *p = &buffer->data[token->position];
                        AutoComplete_Remove(p, token->size);
                    }

                    if(Lex_IsUserToken(token) && token->reserved != nullptr){
                        SymbolTable_Remove(symTable, (char *)token->reserved,
                        token->size, token->identifier);
                        AllocatorFree(token->reserved);
                        token->reserved = nullptr;
                    }
                }
            }
        }

        LineBuffer_RemountBuffer(lineBuffer, buffer, tokenizer, i);
        buffer->erased = false;

        i++;
        if(i >= lineBuffer->lineCount) break;
    }

    activeLineBuffer = nullptr;
    currentID = 0;
    Lex_TokenizerSetFetchCallback(tokenizer, nullptr);
}

void LineBuffer_Init(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                     char *fileContents, uint filesize, bool synchronous)
{
    AssertA(lineBuffer != nullptr && fileContents != nullptr && filesize > 0,
            "Invalid line buffer initialization");

    if(synchronous){
        LineBufferTokenizer lineBufferTokenizer;
        LineBuffer_InitBlank(lineBuffer);

        lineBufferTokenizer.tokenizer = tokenizer;
        lineBufferTokenizer.lineBuffer = lineBuffer;
        lineBufferTokenizer.lineBacktrack = 0;
        lineBufferTokenizer.func = [&](int lineno) -> void{};

        activeLineBuffer = lineBuffer;
        current = 0;
        totalSize = filesize;
        content = fileContents;

        Lex_TokenizerSetFetchCallback(tokenizer, LineBuffer_TokenizerFileFetcher);

        Lex_LineProcess(fileContents, filesize, LineBuffer_LineProcessor,
                        0, &lineBufferTokenizer, true);

        activeLineBuffer = nullptr;
        current = 0;
        totalSize = 0;
        content = nullptr;

        Lex_TokenizerSetFetchCallback(tokenizer, nullptr);
    }else{ // Make the file tokenization run in a different thread
        DispatchExecution([&](HostDispatcher *dispatcher){
            // becarefull with pointers and variables as the parent
            // context will dissolve once we dispatch the host.
            const int kReleaseHostAt = 500;
            LineBuffer *localLineBufferPtr = lineBuffer;
            Tokenizer *localTokenizerPtr = tokenizer;

            LineBufferTokenizer lineBufferTokenizer;
            LineBuffer_InitBlank(localLineBufferPtr);
            LineBuffer_SetWrittable(localLineBufferPtr, false);

            lineBufferTokenizer.tokenizer = localTokenizerPtr;
            lineBufferTokenizer.lineBuffer = localLineBufferPtr;
            lineBufferTokenizer.lineBacktrack = 0;
            lineBufferTokenizer.func = [&](int lineno) -> void{
                if(lineno > kReleaseHostAt && !dispatcher->IsDispatched()){
                    // we parsed enough, release the caller
                    dispatcher->DispatchHost();
                }
            };

            activeLineBuffer = localLineBufferPtr;
            current = 0;
            totalSize = filesize;
            content = fileContents;

            Lex_TokenizerSetFetchCallback(localTokenizerPtr, LineBuffer_TokenizerFileFetcher);

            Lex_LineProcess(fileContents, filesize, LineBuffer_LineProcessor,
                            0, &lineBufferTokenizer, true);

            LineBuffer_SetWrittable(localLineBufferPtr, true);

            activeLineBuffer = nullptr;
            current = 0;
            totalSize = 0;
            content = nullptr;

            Lex_TokenizerSetFetchCallback(localTokenizerPtr, nullptr);
            // release the caller, just in case we haven't released it before
            dispatcher->DispatchHost();
        });
    }
}

uint LineBuffer_InsertRawTextAt(LineBuffer *lineBuffer, char *text, uint size,
                                uint base, uint u8offset, uint *offset,
                                int replaceDashR)
{
    /*
    * Algorithm: We need to insert a possible multi-line text at a given position.
    * We need to get the content from the position forward in the buffer. Move all
    * the buffers bellow to the amount of lines inserted. Add the first line to the
    * the original buffer, insert the rest of the lines and finally the original
    * text from the position. Because we will make multiple line movements
    * it is faster if we directly move the buffers than to perform line insertion
    * with 'LineBuffer_InsertLineAt'.
    */

    // 1 - Count the amount of lines so we can shift the file in a single pass
    //     the amount of lines is given by the text and not the buffer so its ok
    //     to check for that only.
    uint nLines = 0;
    uint bId = base;
    Buffer *lastBuffer = lineBuffer->lines[base];
    char *firstLine = nullptr;
    for(uint i = 0; i < size; i++){
        if(text[i] == '\n') nLines++;
    }

    //LineBuffer_DebugPrintRange(lineBuffer, vec2i((int)base-2, nLines+2));

    // 2 - Create nLines buffers for the file
    if(!(lineBuffer->lineCount + nLines < lineBuffer->size && lineBuffer->size > base)){
        uint newsize = lineBuffer->lineCount + nLines;
        uint offset = (lineBuffer->size > base ? 0 : base - lineBuffer->size) + lineBuffer->size;
        newsize = Max(offset, newsize) + DefaultAllocatorSize;
        lineBuffer->lines = AllocatorExpand(Buffer *, lineBuffer->lines,
                                            newsize, lineBuffer->size);

        for(uint i = 0; i < newsize - lineBuffer->size; i++){
            lineBuffer->lines[lineBuffer->size+i] = (Buffer *)AllocatorGet(sizeof(Buffer));
            *(lineBuffer->lines[lineBuffer->size+i]) = BUFFER_INITIALIZER;
        }

        lineBuffer->size = newsize;
    }

    // 3 - Move the buffer to allow nLines inserted
    uint endp   = lineBuffer->lineCount + nLines - 1;
    uint startp = lineBuffer->lineCount - 1;
    while(startp > base){
        Buffer *bufferEnd = lineBuffer->lines[endp];
        Buffer *bufferSt  = lineBuffer->lines[startp];
        Buffer_CopyReferences(bufferEnd, bufferSt);
        if(!(startp > base + nLines)){
            Buffer_Release(bufferSt);
        }
        startp--;
        endp--;
    }

    // 4 - Insert the new lines
    char *lineStart = text;
    uint at = u8offset;
    int pp = -1;
    uint lineSize = 0;
    uint inc = 0;
    uint proc = 0;
    lastBuffer = lineBuffer->lines[base];
    uint firstP = 0;
    uint toCopy = lastBuffer->taken;

    //TODO: debug mode registered a calloc here with size 0!
    firstLine = (char *)AllocatorGet(sizeof(char) * lastBuffer->taken);
    Memcpy(firstLine, lastBuffer->data, lastBuffer->taken * sizeof(char));

    while(proc < size){
        char s = text[proc];
        if(s == '\r' && replaceDashR){
            text[proc] = '\n';
            s = '\n';
        }

        if(s == '\r') proc++;
        else if(s == '\n'){
            //Buffer *buffer = lineBuffer->lines[base];
            text[proc] = 0;
            lastBuffer = lineBuffer->lines[base];
            Buffer_InsertStringAt(lastBuffer, at, lineStart, lineSize);
            //printf("Inserting block %s at %u ( %u )\n", lineStart, at, base);
            if(pp < 0){
                uint rpos = Buffer_Utf8PositionToRawPosition(lastBuffer, at);
                firstP = rpos;
                toCopy -= rpos;
                Buffer_RemoveRangeRaw(lastBuffer, firstP+lineSize, lastBuffer->taken);
                pp = 0;
            }

            Buffer_Claim(lastBuffer);
            text[proc] = s;
            base++;
            proc++;
            lineStart = &text[proc];
            at = 0;
            lineSize = 0;
        }else{
            proc++;
            lineSize++;
        }
    }

    // If we did not finish at '\n' than manually copy the missing content
    if(lineSize > 0){
        lastBuffer = lineBuffer->lines[base];
        uint n = Buffer_InsertStringAt(lastBuffer, at, lineStart, lineSize);
        Buffer_Claim(lastBuffer);
        at += n;
        //printf("Inserting block %s at %u ( %u )\n", lineStart, at, base);
    }

    AssertErr(lastBuffer != nullptr, "Invalid insertion");

    // 5 - Now copy whatever is missing from the original first line
    if(toCopy > 0){
        if(pp == 0){
            // if we inserted a new line than this must be copied to the final buffer
            at = lastBuffer->taken;
            Buffer_InsertRawStringAt(lastBuffer, lastBuffer->taken,
                                     &firstLine[firstP], toCopy);
            Buffer_Claim(lastBuffer);
        }
    }

    // 6 - In case our copy was empty return to 0
    if(lineSize == 0){
        at = 0;

        // End of file handling
        Buffer *b = lineBuffer->lines[base];
        if(b->data == nullptr) Buffer_Init(b, DefaultAllocatorSize);
    }

    uint lineCount = lineBuffer->lineCount-1;
    lineBuffer->lineCount += (base - bId);

    //printf("Added %u lines\n", base - bId);

    // 7 - Check the file ending is correct as we cannot have nullptrs
    for(uint i = lineCount; i < lineBuffer->lineCount; i++){
        Buffer *b = lineBuffer->lines[i];
        if(b->data == nullptr) Buffer_Init(b, DefaultAllocatorSize);
    }

    *offset = Buffer_Utf8RawPositionToPosition(lastBuffer, at);

    AllocatorFree(firstLine);

    //LineBuffer_DebugPrintRange(lineBuffer, vec2i((int)bId-2, nLines+20));
    return base - bId + inc;
}

void LineBuffer_InitEmpty(LineBuffer *lineBuffer){
    LineBuffer_InitBlank(lineBuffer);
    LineBuffer_InsertLine(lineBuffer, nullptr, 0);
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, 0);
    Lex_TokenizerContextEmpty(&buffer->stateContext);
}

void LineBuffer_CopyLineTokens(LineBuffer *lineBuffer, uint lineNo,
                               Token *tokens, uint size)
{
    Buffer *buffer = nullptr;
    AssertA(lineBuffer != nullptr && lineNo >= 0 && lineNo < lineBuffer->lineCount,
            "Invalid input given");
    buffer = lineBuffer->lines[lineNo];

    Buffer_UpdateTokens(buffer, tokens, size);
}

void LineBuffer_Free(LineBuffer *lineBuffer){
    if(lineBuffer){
#if defined(MEMORY_DEBUG)
        if(lineBuffer->lines && lineBuffer->size == 0){
            printf("[MEM] Linebuffer has lines but 0 size\n");
            getchar();
        }
#endif
        for(int i = lineBuffer->size-1; i >= 0; i--){
            Buffer_Free(lineBuffer->lines[i]);
            AllocatorFree(lineBuffer->lines[i]);
        }
        if(lineBuffer->lines && lineBuffer->size > 0)
            AllocatorFree(lineBuffer->lines);

        UndoRedoCleanup(&lineBuffer->undoRedo);

        AllocatorFree(lineBuffer->undoRedo.undoStack);
        AllocatorFree(lineBuffer->undoRedo.redoStack);

        lineBuffer->lines = nullptr;
        lineBuffer->size = 0;
        lineBuffer->lineCount = 0;
    }
}

Buffer *LineBuffer_GetBufferAt(LineBuffer *lineBuffer, uint lineNo){
    if(lineBuffer){
        if(lineNo < lineBuffer->lineCount){
            return lineBuffer->lines[lineNo];
        }
    }

    return nullptr;
}

char *LineBuffer_GetStoragePath(LineBuffer *lineBuffer){
    return &lineBuffer->filePath[0];
}

void LineBuffer_SetStoragePath(LineBuffer *lineBuffer, char *path, uint size){
    if(path && size > 0){
        strncpy(lineBuffer->filePath, path, Min(size, sizeof(lineBuffer->filePath)));
        lineBuffer->filePathSize = strlen(lineBuffer->filePath);
    }else{
        lineBuffer->filePathSize = 0;
        lineBuffer->filePath[0] = 0;
    }
}

void LineBuffer_SetWrittable(LineBuffer *lineBuffer, bool isWrittable){
    if(lineBuffer){
        lineBuffer->props.isWrittable = isWrittable;
    }else{
        printf("Warning: Attempted to set null linebuffer writtable flag\n");
    }
}

bool LineBuffer_IsWrittable(LineBuffer *lineBuffer){
    if(lineBuffer){
        return lineBuffer->props.isWrittable;
    }
    // if no linebuffer is given it is potentially a flow execution
    // for outer components (autocomplete, querybar, ...) in this
    // case do not reject the caller.
    return true;
}

uint LineBuffer_GetTextFromRange(LineBuffer *lineBuffer, char **ptr,
                                 vec2ui start, vec2ui end)
{
    uint lines = lineBuffer->lineCount;
    char *data = nullptr;
    uint unprocessedSize = 0;
    uint ei = end.x < lines ? end.x : lines-1;
    uint si = start.x;
    uint spi = 0;
    uint maxi = 0;
	uint size = 0;
    Buffer *b = LineBuffer_GetBufferAt(lineBuffer, si);
    uint pi = Buffer_Utf8PositionToRawPosition(b, start.y);
    spi = pi;
    do{
        unprocessedSize += b->taken - pi + 1;
        if(si == ei){
            pi = Buffer_Utf8PositionToRawPosition(b, end.y);
            maxi = pi;
        }

        si++;
        if(si <= ei){
            b = LineBuffer_GetBufferAt(lineBuffer, si);
            pi = si < ei ? 0 : pi;
        }
    }while(si <= ei);

    data = AllocatorGetN(char, unprocessedSize);
    uint ic = 0;
    uint c = spi;
	size = unprocessedSize;
    for(uint i = start.x; i <= ei; i++){
        b = LineBuffer_GetBufferAt(lineBuffer, i);
        char *p = &b->data[c];
        uint m = (i == ei) ? maxi : b->taken;

        // small hack for empty lines at the end of a copy
        //if(m == 0 && i == ei) data[ic++] = '\n';

        while(m > c){
            if(!(ic < size)){
                data = AllocatorExpand(char, data, size+DefaultAllocatorSize, size);
                size += DefaultAllocatorSize;
            }
            data[ic++] = *p;
            AssertA(*p != '\n', "Invalid line configuration {break point}");
            p++; c++;
        }

        if(i < ei){
            if(!(ic < size)){
                data = AllocatorExpand(char, data, size+DefaultAllocatorSize, size);
                size += DefaultAllocatorSize;
            }
            data[ic++] = '\n';
        }
        c = 0;
    }

	if(!(ic < size)){
		data = AllocatorExpand(char, data, size+DefaultAllocatorSize, size);
		size += DefaultAllocatorSize;
	}

    AssertA(ic < size, "Invalid write size");
    data[ic++] = 0;
    *ptr = data;
    return ic-1;
}

Buffer *LineBuffer_ReplaceBufferAt(LineBuffer *lineBuffer, Buffer *buffer, uint at){
    Buffer *b = nullptr;
    if(at < lineBuffer->size){
        b = lineBuffer->lines[at];
        lineBuffer->lines[at] = buffer;
    }
    return b;
}

void LineBuffer_SetActiveBuffer(LineBuffer *lineBuffer, vec2i bufId, int safe){
    uint i = lineBuffer->activeBuffer.x;
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
    if(buffer && (int)i != bufId.x && safe){
        Buffer_RemoveExcessSpace(buffer);
    }
    lineBuffer->activeBuffer = bufId;
}

vec2i LineBuffer_GetActiveBuffer(LineBuffer *lineBuffer){
    return lineBuffer->activeBuffer;
}

static bool saveDispatchRunning = false;
void LineBuffer_SaveToStorage(LineBuffer *lineBuffer){
    if(lineBuffer->filePathSize < 1){
        printf("Skipping un-writtable linebuffer\n");
        return;
    }

    StorageDevice *storage = FetchStorageDevice();
    const long pollTimeout = 100;
    long timeTaken = 0;

    // Whenever we save remotely we need to make sure that any previous
    // writes are complete because the server is not asynchronous. And if
    // connection is slow it might happen that dispatching another save
    // will corrupt the first one because dispatching spawns a different
    // thread and the dispatching routine shares host dispatchers based on the
    // the type of dispatch that is happening. So I'll wait at most 2 seconds
    // to see if any previous dispatches finishes, if not we have no choice but
    // to refuse this write and force the user to request again later.

    // TODO: It might be worth to store the time the dispatch was made
    //       to make sure this is indeed a dispatch running and not a bug.
    //       However attempting to detect a slow connection/bug on socket
    //       is very tricky.
    if(!storage->IsLocallyStored()){
        while(saveDispatchRunning && timeTaken < 2000){
            std::this_thread::sleep_for(std::chrono::milliseconds(pollTimeout));
            timeTaken += pollTimeout;
        }

        if(saveDispatchRunning){
            LOG_ERR("Cannot save file, dispatch still running. Retry later.");
            lineBuffer->is_dirty = 1;
            return;
        }
    }

    FileHandle file;
    // using a std::sttringstream for buffering the content
    // is better for remote usage
    std::stringstream ss;
    uint lines = lineBuffer->lineCount;
    if(!storage->StreamWriteStart(&file, lineBuffer->filePath)){
        printf("Could not open file %s in current storage device\n",
               lineBuffer->filePath);
        return;
    }

    uint maxSize = 0;
    uint ic = 0;
    char *linePtr = nullptr;
    uint i = 0;

    for(uint i = 0; i < lines; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        maxSize = Max(maxSize, buffer->taken);
    }

    linePtr = AllocatorGetN(char, maxSize+2);

    for(i = 0; i < lines; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        char *ptr = buffer->data;
        bool process = true;
        uint c = 0;
        uint taken = buffer->taken;
        ic = 0;
        if(buffer->is_ours){
            // skip visualy empty lines and remove pending spaces at end of the line
            process = !Buffer_IsBlank(buffer);
            if(process){
                // remove spaces at end of line also
                for(taken = buffer->taken; taken > 1; taken--){
                    if(buffer->data[taken-1] != ' ') break;
                }
            }
        }

        while(taken > c && process){
            linePtr[ic++] = *ptr;
            AssertA(*ptr != '\n', "Invalid line configuration {break point}");
            ptr++; c++;
        }

        linePtr[ic++] = '\n';
        // this is not really a bug it just means we don't have space for \0
        if(ic+1 >= maxSize+2){
            //printf("Invalid write\n");
        }else{
            linePtr[ic+1] = 0;
        }

        ss << std::string(linePtr, ic);
        //uint s = storage->StreamWriteBytes(&file, linePtr, sizeof(char), ic);
        //(void)s;
        //AssertA(s == ic, "Failed to write to file");
    }

    AllocatorFree(linePtr);
    lineBuffer->is_dirty = 0;

    std::string v = ss.str();
    if(!storage->IsLocallyStored()){
        DispatchExecution([&](HostDispatcher *dispatcher){
            std::string localStr = v;
            FileHandle localFile = file;
            StorageDevice *localStorage = storage;
            saveDispatchRunning = true;

            dispatcher->DispatchHost();

            localStorage->StreamWriteBytes(&localFile, (void *)localStr.c_str(),
                                           1, localStr.size());
            localStorage->StreamFinish(&localFile);
            saveDispatchRunning = false;
        });
    }else{
        storage->StreamWriteBytes(&file, (void *)v.c_str(), 1, v.size());
        storage->StreamFinish(&file);
    }
}

int LineBuffer_IsInsideCopySection(LineBuffer *lineBuffer, uint id, uint bid){
    int rv = 0;
    if(lineBuffer){
        CopySection *section = &lineBuffer->props.cpSection;
        if(section->active && bid >= section->start.x && bid <= section->end.x){
            if(bid < section->end.x) return 1;

            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, bid);
            if(id < buffer->tokenCount){
                Token *token = &buffer->tokens[id];
                uint s = token->position + token->size;
                uint ps = Buffer_Utf8RawPositionToPosition(buffer, token->position);
                uint pe = Buffer_Utf8RawPositionToPosition(buffer, s);
                uint start = section->start.x == section->end.x ? section->start.y : 0;
                uint end = section->end.x == bid ? section->end.y : buffer->count;
                if(ps >= start && pe <= end){
                    rv = 1;
                }
            }
        }
    }

    return rv;
}

void LineBuffer_AdvanceCopySection(LineBuffer *lineBuffer, double dt){
    if(lineBuffer){
        CopySection *section = &lineBuffer->props.cpSection;
        if(section->active){
            section->currTime += dt;
            if(section->currTime > section->interval){
                section->active = 0;
            }
        }
    }
}

LineBuffer *LineBuffer_AllocateInternal(LineBuffer *lBuffer){
    LineBuffer *lineBuffer = lBuffer;
    if(!lineBuffer){
        lineBuffer = AllocatorGetN(LineBuffer, 1);
    }
    LineBuffer_InitEmpty(lineBuffer);
    LineBuffer_SetStoragePath(lineBuffer, nullptr, 0);
    LineBuffer_SetWrittable(lineBuffer, false);
    LineBuffer_SetType(lineBuffer, 2); // 2 = Empty tokenizer
    LineBuffer_SetExtension(lineBuffer, FILE_EXTENSION_NONE);
    LineBuffer_SetInternal(lineBuffer);
    return lineBuffer;
}

void LineBuffer_SetExtension(LineBuffer *lineBuffer, FileExtension ext){
    if(lineBuffer){
        lineBuffer->props.ext = ext;
    }
}

void LineBuffer_SetType(LineBuffer *lineBuffer, uint type){
    if(lineBuffer){
        lineBuffer->props.type = type;
    }
}

void LineBuffer_SetCopySection(LineBuffer *lineBuffer, CopySection section){
    if(lineBuffer){
        lineBuffer->props.cpSection = section;
    }
}

void LineBuffer_SetInternal(LineBuffer *lineBuffer){
    if(lineBuffer){
        lineBuffer->props.isInternal = true;
    }
}

bool LineBuffer_IsInternal(LineBuffer *lineBuffer){
    bool rv = false;
    if(lineBuffer){
        rv = lineBuffer->props.isInternal;
    }
    return rv;
}

FileExtension LineBuffer_GetExtension(LineBuffer *lineBuffer){
    if(lineBuffer){
        return lineBuffer->props.ext;
    }

    return FILE_EXTENSION_NONE;
}

uint LineBuffer_GetType(LineBuffer *lineBuffer){
    if(lineBuffer){
        return lineBuffer->props.type;
    }

    return 0;
}

void LineBuffer_GetCopySection(LineBuffer *lineBuffer, CopySection *section){
    if(lineBuffer && section){
        *section = lineBuffer->props.cpSection;
    }
}

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer){
#if defined(BUG_HUNT)
    printf("Current UTF-8 size: %u\n", buffer->count);
    printf("Current Size: %u\n", buffer->size);
    for(uint j = 0; j < buffer->taken; j++){
        printf("%c", buffer->data[j]);
    }

    printf("\n");
    for(uint j = 0; j < buffer->taken; j++){
        printf("%d ", (int)buffer->data[j]);
    }

    printf("\n");
    printf(" TOKENS (%d): ", buffer->tokenCount);
    for(uint j = 0; j < buffer->tokenCount; j++){
        Token *t = &buffer->tokens[j];
        char *s = &buffer->data[t->position];
        char x  = s[t->size];
        s[t->size] = 0;
        printf("\x1B[34m( %d/%d \x1B[37m" "%s" "\x1B[34m )\x1B[37m",t->position, t->size,s);
        s[t->size] = x;
    }
#endif
}

void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo){
#if defined(BUG_HUNT)
    if(lineNo < lineBuffer->lineCount){
        Buffer *buffer = lineBuffer->lines[lineNo];
        printf("[ %d ] : ", lineNo+1);
        Buffer_DebugStdoutData(buffer);
    }
#endif
}

void LineBuffer_DebugPrintRange(LineBuffer *lineBuffer, vec2i range){
#if defined(BUG_HUNT)
    int start = Max(0, range.x);
    uint end = Min(range.x + range.y+1, lineBuffer->lineCount);

    printf("Range %d %d\n", start, end);
    if(start > 0){
        printf(" ( ... )\n");
    }

    for(uint i = start; i < end; i++){
        Buffer *buffer = lineBuffer->lines[i];
        printf("(%u) %d - %s\n", buffer->tokenCount, i, buffer->data);
    }

    if(end < lineBuffer->lineCount){
        printf(" ( ... )\n");
    }
#endif
}

uint LineBuffer_DebugLoopAllTokens(LineBuffer *lineBuffer, const char *m, uint size){
#if defined(BUG_HUNT)
    uint n = lineBuffer->lineCount;
    uint count = 0;
    for(uint i = 0; i < n; i++){
        Buffer *buffer = lineBuffer->lines[i];
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(token->identifier == TOKEN_ID_NONE && (uint)token->size == size){
                if(StringEqual(&buffer->data[token->position], (char *)m, size)){
                    count ++;
                }
            }
            //printf("( %s ) ", Lex_GetIdString(token->identifier));
        }
        //printf("\n");
    }

    return count;
#else
    return 0;
#endif
}
