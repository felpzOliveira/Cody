#include <buffers.h>
#include <maths.h>

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
    }
}

void Buffer_Init(Buffer *buffer, uint size){
    AssertA(buffer != nullptr && size > 0, "Invalid buffer initialization");
    buffer->data = (char *)AllocatorGet(size * sizeof(char));
    buffer->size = size;
    buffer->count = 0;
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
    
    _memcpy(buffer->data, head, sizeof(char) * len);
    buffer->count = len;
}

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
        Buffer_Release(buffer);
    }
}

void LineBuffer_InsertLine(LineBuffer *lineBuffer, char *line, uint size){
    AssertA(lineBuffer != nullptr && line != nullptr && size > 0,
            "Invalid line initialization");
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

void LineBuffer_Init(LineBuffer *lineBuffer, const char *fileContents, uint filesize){
    AssertA(lineBuffer != nullptr && fileContents != nullptr && filesize > 0,
            "Invalid line buffer initialization");
    
    char *pStart = (char *)fileContents;
    char *p = pStart;
    lineBuffer->lines = AllocatorGetDefault(Buffer);
    lineBuffer->lineCount = 0;
    lineBuffer->size = DefaultAllocatorSize;
    
    for(uint i = 0; i < filesize; i++){
        if(*p == '\r'){
            if(*(p+1) == '\n'){
                p++;
                i++;
                LineBuffer_InsertLine(lineBuffer, pStart, p - pStart);
                pStart = p;
            }
            
            p++;
        }else if(*p == '\n'){
            p++;
            LineBuffer_InsertLine(lineBuffer, pStart, p - pStart);
            pStart = p;
        }else{
            p++;
        }
    }
    
    printf("Got %d lines\n", lineBuffer->lineCount);
#if 1
    for(int i = 0; i < lineBuffer->lineCount; i++){
        Buffer *line = &lineBuffer->lines[i];
        printf("[ %d ] : ", i+1);
        Buffer_DebugStdoutData(line);
    }
#endif
}

void LineBuffer_Free(LineBuffer *lineBuffer){
    if(lineBuffer){
        for(int i = lineBuffer->lineCount-1; i >= 0; i--){
            Buffer_Free(&lineBuffer->lines[i]);
        }
        if(lineBuffer->lines)
            AllocatorFree(lineBuffer->lines);
        
        lineBuffer->lines = nullptr;
        lineBuffer->lineCount = 0;
        lineBuffer->size = 0;
    }
}

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer){
    for(int j = 0; j < buffer->count; j++){
        printf("%c", buffer->data[j]);
    }
}

void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo){
    if(lineNo < lineBuffer->lineCount){
        Buffer *buffer = &lineBuffer->lines[lineNo];
        printf("[ %d ] : ", lineNo+1);
        Buffer_DebugStdoutData(buffer);
    }
}