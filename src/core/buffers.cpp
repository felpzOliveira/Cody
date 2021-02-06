#include <buffers.h>
#include <geometry.h>
#include <time.h>
#include <utilities.h>

#define MODULE_NAME "Buffer"

void Buffer_CopyReferences(Buffer *dst, Buffer *src){
    if(dst != nullptr && src != nullptr){
        dst->data = src->data;
        dst->count = src->count;
        dst->taken = src->taken;
        dst->size = src->size;
        dst->tokenCount = src->tokenCount;
        dst->tokens = src->tokens;
        dst->stateContext = src->stateContext;
    }
}

uint Buffer_Utf8RawPositionToPosition(Buffer *buffer, uint rawp){
    AssertA(rawp <= buffer->taken, "Impossible query");
    uint r = 0;
    if(buffer->taken > 0){
        char *p = buffer->data;
        int c = 0;
        int of = 0;
        while(c != (int)rawp && buffer->taken > c){
            of = 0;
            int rv = StringToCodepoint(&p[c], buffer->taken - c, &of);
            if(rv == -1) break;
            c += of;
            r++;
        }
        
        AssertA(c == (int)rawp, "StringToCodepoint failed to decode UTF-8");
    }
    
    return r;
}

uint Buffer_Utf8PositionToRawPosition(Buffer *buffer, uint u8p, int *len){
    AssertA(u8p <= buffer->taken, "Impossible query");
    uint r = 0;
    if(buffer->taken > 0){
        char *p = buffer->data;
        int c = 0;
        int of = 0;
        
        if(u8p == 0 && len){
            int rv = StringToCodepoint(&p[c], buffer->taken - c, &of);
            *len = of;
            return 0;
        }
        
        while(r != u8p){
            of = 0;
            int rv = StringToCodepoint(&p[c], buffer->taken - c, &of);
            if(rv == -1){
                break;
            }
            
            r++;
            c += of;
        }
        
        AssertA(r == u8p, "StringToCodepoint failed to decode UTF-8");
        if(len){
            *len = 1;
            if(buffer->taken > c){
                (void)StringToCodepoint(&p[c], buffer->taken - c, &of);
                *len = of;
            }
        }
        r = (uint)c;
    }else{
        if(len) *len = 1;
    }
    
    return r;
}

uint Buffer_GetUtf8Count(Buffer *buffer){
    uint r = 0;
    if(buffer->taken > 0){
        char *p = buffer->data;
        int c = 0;
        int rv = -1;
        do{
            int of = 0;
            rv = StringToCodepoint(&p[c], buffer->taken - c, &of);
            if(rv != -1){
                c += of;
                r += 1;
            }
        }while(*p && c < buffer->taken && rv >= 0);
    }
    
    return r;
}

void Buffer_UpdateTokens(Buffer *buffer, Token *tokens, uint size){
    if(buffer){
        if(buffer->tokenCount < size){
            if(buffer->tokens){
                buffer->tokens = AllocatorExpand(Token, buffer->tokens, size);
            }else{
                buffer->tokens = AllocatorGetN(Token, size);
            }
        }
        
        buffer->tokenCount = size;
        Memcpy(buffer->tokens, tokens, size * sizeof(Token));
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
}

void Buffer_InitSet(Buffer *buffer, char *head, uint len){
    if(buffer->data == nullptr){
        buffer->data = (char *)AllocatorGet((len+DefaultAllocatorSize) * sizeof(char));
        buffer->size = len+DefaultAllocatorSize;
    }else{
        if(buffer->size < len){
            uint newSize = buffer->size + len + DefaultAllocatorSize;
            buffer->data = AllocatorExpand(char, buffer->data, newSize);
            buffer->size = newSize;
        }
    }
    
    if(len > 0){
        Memcpy(buffer->data, head, sizeof(char) * len);
    }
    buffer->taken = len;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
    buffer->stateContext.state = TOKENIZER_STATE_CLEAN;
    buffer->stateContext.activeWorkProcessor = -1;
    buffer->stateContext.backTrack = 0;
    buffer->stateContext.forwardTrack = 0;
    buffer->count = Buffer_GetUtf8Count(buffer);
}

int Buffer_IsBlank(Buffer *buffer){
    if(buffer->count == 0) return 1;
    for(int i = 0; i < buffer->count; i++){
        char p = buffer->data[i];
        if(p != ' ' && p != '\r' && p != '\t' && p != '\n') return 0;
    }
    
    return 1;
}

void Buffer_RemoveRange(Buffer *buffer, uint u8start, uint u8end){
    if(u8end > u8start){
        int endSize = 0;
        uint start = Buffer_Utf8PositionToRawPosition(buffer, u8start, nullptr);
        uint end = Buffer_Utf8PositionToRawPosition(buffer, u8end, &endSize);
        uint endLoc = Min(buffer->taken, end + endSize - 1);
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

void Buffer_InsertStringAt(Buffer *buffer, uint rap, char *str, uint len){
    uint at = Buffer_Utf8PositionToRawPosition(buffer, rap);
    if(buffer->size < at){
        int diff = at - buffer->size;
        uint newSize = diff + len + DefaultAllocatorSize;
        buffer->data = AllocatorExpand(char, buffer->data, newSize);
    }
    
    if(buffer->size < buffer->taken + len){
        uint newSize = buffer->size + len + DefaultAllocatorSize;
        buffer->data = AllocatorExpand(char, buffer->data, newSize);
        buffer->size = newSize;
    }
    
    if(buffer->taken > at){
        uint shiftSize = buffer->taken - at + 1;
        for(uint i = 0; i < shiftSize; i++){
            buffer->data[buffer->taken-i+len] = buffer->data[buffer->taken-i];
        }
    }
    
    for(int i = 0; i < len; i++){
        buffer->data[at+i] = str[i];
    }
    
    buffer->taken += len;
    buffer->count = Buffer_GetUtf8Count(buffer);
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

void Buffer_Free(Buffer *buffer){
    if(buffer){
        if(buffer->data) AllocatorFree(buffer->data);
        if(buffer->tokens) AllocatorFree(buffer->tokens);
        Buffer_Release(buffer);
    }
}

void LineBuffer_InsertLine(LineBuffer *lineBuffer, char *line, uint size){
    AssertA(lineBuffer != nullptr, "Invalid line initialization");
    if(!(lineBuffer->lineCount < lineBuffer->size)){
        uint newSize = lineBuffer->size + DefaultAllocatorSize;
        lineBuffer->lines = AllocatorExpand(Buffer, lineBuffer->lines, newSize);
        
        for(uint i = 0; i < DefaultAllocatorSize; i++){
            lineBuffer->lines[lineBuffer->size+i] = BUFFER_INITIALIZER;
        }
        
        lineBuffer->size = newSize;
    }
    if(size > 0 && line)
        Buffer_InitSet(&lineBuffer->lines[lineBuffer->lineCount], line, size);
    else
        Buffer_Init(&lineBuffer->lines[lineBuffer->lineCount], DefaultAllocatorSize);
    lineBuffer->lineCount++;
}

void LineBuffer_RemoveLineAt(LineBuffer *lineBuffer, uint at){
    if(lineBuffer->lineCount > at){
        // swap pointers forward from 'at'
        Buffer *Li = &lineBuffer->lines[at];
        Buffer_Free(Li);
        for(uint i = at; i < lineBuffer->lineCount-1; i++){
            // Line N receives the pointer from the next line N+1
            Buffer *lineN   = &lineBuffer->lines[i];
            Buffer *lineNp1 = &lineBuffer->lines[i+1];
            Buffer_CopyReferences(lineN, lineNp1);
        }
        
        // we need to release the pointers from the final line as it now
        // belongs to line located at 'lineCount-1'
        Buffer_Release(&lineBuffer->lines[lineBuffer->lineCount-1]);
        lineBuffer->lineCount--;
    }
}

void LineBuffer_MergeConsecutiveLines(LineBuffer *lineBuffer, uint base){
    if(base+1 < lineBuffer->lineCount){
        Buffer *Li = LineBuffer_GetBufferAt(lineBuffer, base);
        Buffer *Li1 = LineBuffer_GetBufferAt(lineBuffer, base+1);
        Buffer_InsertStringAt(Li, Li->count, Li1->data, Li1->count);
        LineBuffer_RemoveLineAt(lineBuffer, base+1);
    }
}

void LineBuffer_InsertLineAt(LineBuffer *lineBuffer, uint at, char *line, uint size){
    Buffer *Li = nullptr;
    // make sure we can hold at least one more line
    if(!(lineBuffer->lineCount < lineBuffer->size)){
        uint newSize = lineBuffer->size + DefaultAllocatorSize;
        lineBuffer->lines = AllocatorExpand(Buffer, lineBuffer->lines, newSize);
        
        for(uint i = 0; i < DefaultAllocatorSize; i++){
            lineBuffer->lines[lineBuffer->size+i] = BUFFER_INITIALIZER;
        }
        
        lineBuffer->size = newSize;
    }
    
    if(lineBuffer->lineCount <= at){ // insert at the end of the linebuffer
        Li = &lineBuffer->lines[lineBuffer->lineCount];
    }else{
        // swap pointers from the end until reach 'at'
        for(uint i = lineBuffer->lineCount; i > at; i--){
            // Line N receives the pointer from the previous line N-1
            Buffer *lineN   = &lineBuffer->lines[i];
            Buffer *lineNm1 = &lineBuffer->lines[i-1];
            Buffer_CopyReferences(lineN, lineNm1);
        }
        
        // once we reach the line 'at' we create a new line at this location
        Li = &lineBuffer->lines[at];
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
    lineBuffer->lines = AllocatorGetDefault(Buffer);
    lineBuffer->lineCount = 0;
    lineBuffer->size = DefaultAllocatorSize;
}

typedef struct{
    Tokenizer *tokenizer;
    LineBuffer *lineBuffer;
    int lineBacktrack;
}LineBufferTokenizer;

#define DEBUG_TOKENS  0
static void LineBuffer_LineProcessor(char **p, uint size, uint lineNr, void *prv){
    LineBufferTokenizer *lineBufferTokenizer = (LineBufferTokenizer *)prv;
    Tokenizer *tokenizer = lineBufferTokenizer->tokenizer;
    LineBuffer *lineBuffer = lineBufferTokenizer->lineBuffer;
    TokenizerWorkContext *workContext = tokenizer->workContext;
    
    TokenizerStateContext tokenizerContext;
    Lex_TokenizerGetCurrentState(tokenizer, &tokenizerContext);
    
    LineBuffer_InsertLine(lineBuffer, *p, size);
    workContext->workTokenListHead = 0;
    
    Lex_TokenizerPrepareForNewLine(tokenizer);
#if DEBUG_TOKENS != 0
    char *s = *p;
    printf("==============================================\n");
    printf("{ %d }: %s\n", (int)lineNr, *p);
    printf("==============================================\n");
#endif
    int iSize = size;
    do{
        Token token;
        int rc = Lex_TokenizeNext(p, iSize, &token, tokenizer);
        if(rc < 0){
            iSize = 0;
        }else{
            uint head   = workContext->workTokenListHead;
            uint length = workContext->workTokenListSize;
            if(head >= length){
                length += 32;
                workContext->workTokenList = AllocatorExpand(Token, 
                                                             workContext->workTokenList,
                                                             length);
                workContext->workTokenListSize = length;
            }
            
            workContext->workTokenList[head].size = token.size;
            workContext->workTokenList[head].position = token.position;
            workContext->workTokenList[head].identifier = token.identifier;
            workContext->workTokenListHead++;
#if DEBUG_TOKENS != 0
            char *h = &s[token.position];
            char f = s[token.position+token.size];
            s[token.position+token.size] = 0;
            printf("Token: %s { %s } [%d]\n", h,
                   Lex_GetIdString(token.identifier), token.position);
            s[token.position+token.size] = f;
#endif
            iSize = size - token.position - token.size;
        }
    }while(iSize > 0 && **p != 0);
    
    LineBuffer_CopyLineTokens(lineBuffer, lineNr-1, workContext->workTokenList,
                              workContext->workTokenListHead);
    
    if(Lex_TokenizerHasPendingWork(tokenizer)){
        int r = tokenizerContext.backTrack;
        AssertA(lineNr-1 >= r, "Overflow during forwardtrack computation");
        Buffer *b = LineBuffer_GetBufferAt(lineBuffer, lineNr - 1 - r);
        b->stateContext.forwardTrack = r+2;
    }
    
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, lineNr-1);
    buffer->stateContext = tokenizerContext;
    buffer->stateContext.forwardTrack = 0;
    
}

static void LineBuffer_RemountBuffer(LineBuffer *lineBuffer, Buffer *buffer,
                                     Tokenizer *tokenizer, uint base)
{
    TokenizerStateContext tokenizerContext;
    Lex_TokenizerGetCurrentState(tokenizer, &tokenizerContext);
    
    buffer->stateContext = tokenizerContext;
    buffer->stateContext.forwardTrack = 0;
    char *data = buffer->data;
    
    TokenizerWorkContext *workContext = tokenizer->workContext;
    
    workContext->workTokenListHead = 0;
    
    char *p = buffer->data;
    int totalSize = buffer->taken;
    
    int size = buffer->taken;
    Lex_TokenizerPrepareForNewLine(tokenizer);
    do{
        Token token;
        int rc = Lex_TokenizeNext(&p, size, &token, tokenizer);
        if(rc < 0){
            size = 0;
        }else{
            uint head   = workContext->workTokenListHead;
            uint length = workContext->workTokenListSize;
            if(head >= length){
                length += 32;
                workContext->workTokenList = AllocatorExpand(Token, 
                                                             workContext->workTokenList,
                                                             length);
                workContext->workTokenListSize = length;
            }
            
            workContext->workTokenList[head].size = token.size;
            workContext->workTokenList[head].position = token.position;
            workContext->workTokenList[head].identifier = token.identifier;
            workContext->workTokenListHead++;
            
            size = totalSize - token.position - token.size;
        }
    }while(size > 0 && *p != 0);
    
    Buffer_UpdateTokens(buffer, workContext->workTokenList,
                        workContext->workTokenListHead);
    
    if(Lex_TokenizerHasPendingWork(tokenizer)){
        int r = tokenizerContext.backTrack;
        AssertA(base >= r, "Overflow during forwardtrack computation");
        Buffer *b = LineBuffer_GetBufferAt(lineBuffer, base - r);
        b->stateContext.forwardTrack = r+2;
    }
}

void LineBuffer_ReTokenizeFromBuffer(LineBuffer *lineBuffer, Tokenizer *tokenizer, 
                                     uint base, uint offset)
{
    uint i = 0;
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, base);
    TokenizerStateContext *stateContext = &buffer->stateContext;
    uint start = base - stateContext->backTrack;
    AssertA(start < lineBuffer->lineCount, "BUG: Overflow during backtrack computation");
    
    buffer = LineBuffer_GetBufferAt(lineBuffer, start);
    uint expectedEnd = start + buffer->stateContext.forwardTrack + offset + 1;
    
    Lex_TokenizerRestoreFromContext(tokenizer, &buffer->stateContext);
    
    i = start;
    while((i < expectedEnd || Lex_TokenizerHasPendingWork(tokenizer))){
        buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        LineBuffer_RemountBuffer(lineBuffer, buffer, tokenizer, i);
        
        i++;
        if(i >= lineBuffer->lineCount) break;
    }
}

void LineBuffer_Init(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                     char *fileContents, uint filesize)
{
    LineBufferTokenizer lineBufferTokenizer;
    AssertA(lineBuffer != nullptr && fileContents != nullptr && filesize > 0,
            "Invalid line buffer initialization");
    
    LineBuffer_InitBlank(lineBuffer);
    
    lineBufferTokenizer.tokenizer = tokenizer;
    lineBufferTokenizer.lineBuffer = lineBuffer;
    lineBufferTokenizer.lineBacktrack = 0;
    
    clock_t start = clock();
    
    Lex_LineProcess(fileContents, filesize, LineBuffer_LineProcessor,
                    &lineBufferTokenizer);
    
    clock_t end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    DEBUG_MSG("Lines: %u, Took %g\n\n", lineBuffer->lineCount, taken);
}

void LineBuffer_InitEmpty(LineBuffer *lineBuffer){
    LineBuffer_InitBlank(lineBuffer);
    LineBuffer_InsertLine(lineBuffer, nullptr, 0);
    Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, 0);
    buffer->stateContext = TOKENIZER_STATE_INITIALIZER;
}

void LineBuffer_CopyLineTokens(LineBuffer *lineBuffer, uint lineNo, 
                               Token *tokens, uint size)
{
    Buffer *buffer = nullptr;
    AssertA(lineBuffer != nullptr && lineNo >= 0 && lineNo < lineBuffer->lineCount, 
            "Invalid input given");
    buffer = &lineBuffer->lines[lineNo];
    
    Buffer_UpdateTokens(buffer, tokens, size);
}

void LineBuffer_Free(LineBuffer *lineBuffer){
    if(lineBuffer){
        for(int i = lineBuffer->lineCount-1; i >= 0; i--){
            Buffer_Free(&lineBuffer->lines[i]);
        }
        if(lineBuffer->lines)
            AllocatorFree(lineBuffer->lines);
        
        lineBuffer->lines = nullptr;
        lineBuffer->size = 0;
        lineBuffer->lineCount = 0;
    }
}

Buffer *LineBuffer_GetBufferAt(LineBuffer *lineBuffer, uint lineNo){
    if(lineNo < lineBuffer->lineCount){
        return &lineBuffer->lines[lineNo];
    }
    
    return nullptr;
}

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer){
    for(int j = 0; j < buffer->taken; j++){
        printf("%c", buffer->data[j]);
    }
    
    printf(" TOKENS (%d): ", buffer->tokenCount);
    for(int j = 0; j < buffer->tokenCount; j++){
        Token *t = &buffer->tokens[j];
        char *s = &buffer->data[t->position];
        char x  = s[t->size];
        s[t->size] = 0;
        printf("\x1B[34m( %d/%d \x1B[37m" "%s" "\x1B[34m )\x1B[37m",t->position, t->size,s);
        s[t->size] = x;
    }
}

void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo){
    if(lineNo < lineBuffer->lineCount){
        Buffer *buffer = &lineBuffer->lines[lineNo];
        printf("[ %d ] : ", lineNo+1);
        Buffer_DebugStdoutData(buffer);
    }
}