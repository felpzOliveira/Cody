#include <buffers.h>
#include <geometry.h>
#include <time.h>
#include <utilities.h>
#include <app.h>
#include <string.h>

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

//TODO: Review, this might be showing some issues with lexer
uint Buffer_Utf8RawPositionToPosition(Buffer *buffer, uint rawp){
    if(!(rawp <= buffer->taken)){
        printf("BUG tracked:\n");
        Buffer_DebugStdoutData(buffer);
        printf("Queried for UTF-8 location at %u\n", rawp);
    }
    
    BreakIf(rawp <= buffer->taken, "Impossible query");
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

//TODO: Review, this might be showing some issues with lexer
uint Buffer_Utf8PositionToRawPosition(Buffer *buffer, uint u8p, int *len){
    if(!(u8p <= buffer->taken)){
        printf("BUG tracked:\n");
        Buffer_DebugStdoutData(buffer);
        printf("Queried for UTF-8 translation at %u\n", u8p);
    }
    
    BreakIf(u8p <= buffer->taken, "Impossible query");
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

void Buffer_InitSet(Buffer *buffer, char *head, uint leno, int decode_tab){
    uint len = leno;
    if(decode_tab){
        for(int i = 0; i < leno; i++){
            if(head[i] == '\t'){
                len += appGlobalConfig.tabSpacing - 1;
            }
        }
    }
    
    if(buffer->data == nullptr){
        buffer->size = len+DefaultAllocatorSize;
        buffer->data = (char *)AllocatorGet((buffer->size) * sizeof(char));
    }else{
        if(buffer->size < len){
            uint newSize = buffer->size + len + DefaultAllocatorSize;
            buffer->data = AllocatorExpand(char, buffer->data, newSize);
            buffer->size = newSize;
        }
    }
    
    uint ic = 0;
    if(len > 0){
        for(int i = 0; i < leno; i++){
            if(decode_tab){
                if(head[i] == '\t'){
                    for(int k = 0; k < appGlobalConfig.tabSpacing; k++){
                        buffer->data[ic++] = '\t';
                    }
                }else if(head[i] != '\n'){
                    buffer->data[ic++] = head[i];
                }
            }else{
                if(head[i] != '\n'){
                    buffer->data[ic++] = head[i];
                }
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

void Buffer_InsertStringAt(Buffer *buffer, uint rap, char *str, uint len, int decode_tab){
    uint at = Buffer_Utf8PositionToRawPosition(buffer, rap);
    if(decode_tab){
        for(uint i = 0; i < len; i++){
            if(str[i] == '\t') len += appGlobalConfig.tabSpacing-1;
        }
    }
    
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
        char v = str[i];
        if(decode_tab && v == '\t'){
            for(int k = 0; k < appGlobalConfig.tabSpacing-1; k++)
                buffer->data[at+i] = v;
        }
        
        buffer->data[at+i] = v;
    }
    
    buffer->taken += len;
    buffer->count = Buffer_GetUtf8Count(buffer);
    buffer->data[buffer->taken] = 0;
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

void LineBuffer_InsertLine(LineBuffer *lineBuffer, char *line, uint size, int decode_tab){
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
        Buffer_InitSet(&lineBuffer->lines[lineBuffer->lineCount], line, size, decode_tab);
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
        Buffer_InsertStringAt(Li, Li->count, Li1->data, Li1->taken);
        LineBuffer_RemoveLineAt(lineBuffer, base+1);
    }
}

void LineBuffer_InsertLineAt(LineBuffer *lineBuffer, uint at, char *line, 
                             uint size, int decode_tab)
{
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
        Buffer_InitSet(Li, line, size, decode_tab);
    else
        Buffer_Init(Li, DefaultAllocatorSize);
    lineBuffer->lineCount++;
}

void LineBuffer_InitBlank(LineBuffer *lineBuffer){
    AssertA(lineBuffer != nullptr, "Invalid line buffer blank initialization");
    lineBuffer->lines = AllocatorGetDefault(Buffer);
    lineBuffer->lineCount = 0;
    lineBuffer->is_dirty = 0;
    lineBuffer->size = DefaultAllocatorSize;
}

typedef struct{
    Tokenizer *tokenizer;
    LineBuffer *lineBuffer;
    int lineBacktrack;
}LineBufferTokenizer;


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
    
    LineBuffer_InsertLine(lineBuffer, *p, size-1, 1);
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
    do{
        Token token;
        int rc = Lex_TokenizeNext(p, iSize, &token, tokenizer, 0);
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
            iSize -= rc;
            current += rc;
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
    char *data = buffer->data;
    
    TokenizerWorkContext *workContext = tokenizer->workContext;
    
    workContext->workTokenListHead = 0;
    
    char *p = buffer->data;
    int totalSize = buffer->taken;
    
    int size = buffer->taken;
    Lex_TokenizerPrepareForNewLine(tokenizer, base);
    
    do{
        Token token;
        int rc = Lex_TokenizeNext(&p, size, &token, tokenizer, 1);
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
    Lex_TokenizerSetFetchCallback(tokenizer, LineBuffer_BufferFetcher);
    
    i = start;
    activeLineBuffer = lineBuffer;
    while((i < expectedEnd || Lex_TokenizerHasPendingWork(tokenizer))){
        currentID = i;
        buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        LineBuffer_RemountBuffer(lineBuffer, buffer, tokenizer, i);
        
        i++;
        if(i >= lineBuffer->lineCount) break;
    }
    
    activeLineBuffer = nullptr;
    currentID = 0;
    Lex_TokenizerSetFetchCallback(tokenizer, nullptr);
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
    
    activeLineBuffer = lineBuffer;
    current = 0;
    totalSize = filesize;
    content = fileContents;
    
    Lex_TokenizerSetFetchCallback(tokenizer, LineBuffer_TokenizerFileFetcher);
    
    clock_t start = clock();
    
    Lex_LineProcess(fileContents, filesize, LineBuffer_LineProcessor,
                    0, &lineBufferTokenizer);
    
    clock_t end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    DEBUG_MSG("Lines: %u, Took %g\n\n", lineBuffer->lineCount, taken);
    activeLineBuffer = nullptr;
    current = 0;
    totalSize = 0;
    content = nullptr;
    
    Lex_TokenizerSetFetchCallback(tokenizer, nullptr);
}

uint LineBuffer_InsertRawTextAt(LineBuffer *lineBuffer, char *text, uint size,
                                uint base, uint u8offset, Tokenizer *tokenizer)
{
    //TODO: Lets implement the slow method first, if that sucks too much
    //      implement one that solves both tokens and paste in a single pass
    
    //////// SLOW METHOD
    //1 - Insert by line breaks
    uint processed = 0;
    char *p = text;
    char *lineStart = p;
    uint lineSize = 0;
    uint bufId = base;
    int is_first = 1;
    uint refId = base;
    while(p != NULL && *p != EOF && processed < size){
        if(*p == '\r'){
            p++;
            processed++;
        }
        
        if(*p == '\n'){
            *p = 0;
            if(is_first){ // need to append to buffer
                Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, base);
                Buffer_InsertStringAt(buffer, u8offset, lineStart, lineSize-1, 1);
                is_first = 0;
            }else{
                LineBuffer_InsertLineAt(lineBuffer, base, lineStart, lineSize-1, 1);
            }
            
            p++;
            lineStart = p;
            lineSize = 0;
            processed++;
            base++;
        }else{
            p++;
            lineSize += 1;
            processed++;
        }
    }
    
    if(lineSize > 0){
        if(is_first){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, base);
            Buffer_InsertStringAt(buffer, u8offset, lineStart, lineSize, 1);
        }
        else{
            LineBuffer_InsertLineAt(lineBuffer, base, lineStart, lineSize, 1);
        }
        base++;
    }
    
    //2 - Retokenize this thing now
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, refId, base-refId);
    return base - refId;
}


void LineBuffer_InitEmpty(LineBuffer *lineBuffer){
    LineBuffer_InitBlank(lineBuffer);
    LineBuffer_InsertLine(lineBuffer, nullptr, 0, 1);
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

void LineBuffer_SetStoragePath(LineBuffer *lineBuffer, char *path, uint size){
    strncpy(lineBuffer->filePath, path, Min(size, sizeof(lineBuffer->filePath)));
    lineBuffer->filePathSize = strlen(lineBuffer->filePath);
}

void LineBuffer_SaveToStorage(LineBuffer *lineBuffer){
    uint lines = lineBuffer->lineCount;
    FILE *fp = fopen(lineBuffer->filePath, "wb");
    const char *tab = "\t";
    uint maxSize = 0;
    uint ic = 0;
    char *linePtr = nullptr;
    uint i = 0;
    
    AssertA(fp != NULL, "Failed to open file");
    
    for(uint i = 0; i < lines; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        maxSize = Max(maxSize, buffer->taken);
    }
    
    linePtr = AllocatorGetN(char, maxSize+2);
    
    for(i = 0; i < lines; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        char *ptr = buffer->data;
        uint c = 0;
        ic = 0;
        while(buffer->taken > c){
            linePtr[ic++] = *ptr;
            if(*ptr == '\t'){
                for(int k = 0; k < appGlobalConfig.tabSpacing-1; k++){
                    ptr++; c++;
                    AssertA(*ptr == '\t', "Incorrect line tab configuration");
                }
            }
            
            AssertA(*ptr != '\n', "Invalid line configuration {break point}");
            ptr++; c++;
        }
        
        linePtr[ic++] = '\n';
        linePtr[ic+1] = 0;
        
        uint s = fwrite(linePtr, sizeof(char), ic, fp);
        AssertA(s == ic, "Failed to write to file");
    }
    
    fclose(fp);
    AllocatorFree(linePtr);
    lineBuffer->is_dirty = 0;
}

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer){
    printf("Current UTF-8 size: %u\n", buffer->count);
    printf("Current Size: %u\n", buffer->size);
    for(int j = 0; j < buffer->taken; j++){
        printf("%c", buffer->data[j]);
    }
    
    printf("\n");
    for(int j = 0; j < buffer->taken; j++){
        printf("%d ", (int)buffer->data[j]);
    }
    
    printf("\n");
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

uint LineBuffer_DebugLoopAllTokens(LineBuffer *lineBuffer, const char *m, uint size){
    uint n = lineBuffer->lineCount;
    uint count = 0;
    for(uint i = 0; i < n; i++){
        Buffer *buffer = &lineBuffer->lines[i];
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(token->identifier == TOKEN_ID_NONE && token->size == size){
                if(StringEqual(&buffer->data[token->position], (char *)m, size)){
                    count ++;
                }
            }
            //printf("( %s ) ", Lex_GetIdString(token->identifier));
        }
        //printf("\n");
    }
    
    return count;
}