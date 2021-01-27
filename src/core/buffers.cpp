#include <buffers.h>
#include <geometry.h>
#include <time.h>

#define MODULE_NAME "Buffer"

void _memcpy(void *dst, void *src, uint size){
    unsigned char *udst = (unsigned char *)dst;
    unsigned char *usrc = (unsigned char *)src;
    while(size --){
        *(udst++) = *(usrc++);
    }
}

void Buffer_CopyReferences(Buffer *dst, Buffer *src){
    if(dst != nullptr && src != nullptr){
        dst->data = src->data;
        dst->count = src->count;
        dst->size = src->size;
        dst->tokenCount = src->tokenCount;
        dst->tokens = src->tokens;
    }
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
        _memcpy(buffer->tokens, tokens, size * sizeof(Token));
    }
}

void Buffer_Init(Buffer *buffer, uint size){
    AssertA(buffer != nullptr && size > 0, "Invalid buffer initialization");
    buffer->data = (char *)AllocatorGet(size * sizeof(char));
    buffer->size = size;
    buffer->count = 0;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
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
        _memcpy(buffer->data, head, sizeof(char) * len);
    }
    buffer->count = len;
    buffer->tokens = nullptr;
    buffer->tokenCount = 0;
}

//TODO: Need to update tokens
void Buffer_RemoveRange(Buffer *buffer, uint start, uint end){
    if(end > start){
        uint endLoc = Min(buffer->count, end);
        uint rangeLen = endLoc - start;
        if(buffer->count > rangeLen){
            for(uint i = endLoc, j = 0; i < buffer->count; i++, j++){
                buffer->data[start+j] = buffer->data[i];
            }
            
            buffer->count -= rangeLen;
        }else{
            buffer->count = 0;
        }
    }
}

//TODO: Need to update tokens
void Buffer_InsertStringAt(Buffer *buffer, uint at, char *str, uint len){
    AssertA(buffer->size > at, "Invalid insertion index");
    if(buffer->size < buffer->count + len){
        uint newSize = buffer->size + len + DefaultAllocatorSize;
        buffer->data = AllocatorExpand(char, buffer->data, newSize);
        buffer->size = newSize;
    }
    
    if(buffer->count > at){
        uint shiftSize = buffer->count - at + 1;
        for(uint i = 0; i < shiftSize; i++){
            buffer->data[buffer->count-i+len] = buffer->data[buffer->count-i];
        }
    }
    
    for(int i = 0; i < len; i++){
        buffer->data[at+i] = str[i];
    }
    
    buffer->count += len;
}

void Buffer_Release(Buffer *buffer){
    buffer->data = nullptr;
    buffer->size = 0;
    buffer->count = 0;
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
    
    Buffer_InitSet(&lineBuffer->lines[lineBuffer->lineCount], line, size);
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
    
    Buffer_InitSet(Li, line, size);
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
}LineBufferTokenizer;

#define DEBUG_TOKENS  0
static void LineBuffer_LineProcessor(char **p, uint size, uint lineNr, void *prv){
    LineBufferTokenizer *lineBufferTokenizer = (LineBufferTokenizer *)prv;
    Tokenizer *tokenizer = lineBufferTokenizer->tokenizer;
    LineBuffer *lineBuffer = lineBufferTokenizer->lineBuffer;
    TokenizerWorkContext *workContext = tokenizer->workContext;
    
    LineBuffer_InsertLine(lineBuffer, *p, size);
    workContext->workTokenListHead = 0;
    
    if(**p){
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
    }
    
    LineBuffer_CopyLineTokens(lineBuffer, lineNr-1, workContext->workTokenList,
                              workContext->workTokenListHead);
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
    
    clock_t start = clock();
    
    Lex_LineProcess(fileContents, filesize, LineBuffer_LineProcessor,
                    &lineBufferTokenizer);
    
    clock_t end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    DEBUG_MSG("Lines: %u, Took %g\n\n", lineBuffer->lineCount, taken);
    
#if 0
    for(int i = 0; i < lineBuffer->lineCount; i++){
        LineBuffer_DebugStdoutLine(lineBuffer, i);
        printf("\n");
    }
#endif
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
    for(int j = 0; j < buffer->count; j++){
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