#include <gitbuffer.h>

struct GitLineBuffer{
    LineBuffer *base;
};

GitLineBuffer gitBuffer = { .base = nullptr };

void GitBuffer_InitializeInternalBuffer(){
    LineBuffer *ptr = gitBuffer.base;
    gitBuffer.base = LineBuffer_AllocateInternal(ptr);
}

LineBuffer *GitBuffer_GetLineBuffer(){
    return gitBuffer.base;
}

void GitBuffer_PushLine(char *line, uint size){
    if(gitBuffer.base){
        uint start = gitBuffer.base->lineCount-1;
        LineBuffer_InsertLineAt(gitBuffer.base, start, line, size, 1);
        LineBuffer_FastTokenGen(gitBuffer.base, start, 1);
    }
}

void GitBuffer_Clear(){
    if(gitBuffer.base){
        LineBuffer_Free(gitBuffer.base);
        LineBuffer_AllocateInternal(gitBuffer.base);
    }
}
