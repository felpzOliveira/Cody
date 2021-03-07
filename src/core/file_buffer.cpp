#include <file_buffer.h>
#include <utilities.h>
#include <types.h>

static int FileBufferList_FindEntry(FileBufferList *list, FileBuffer **fBuffer,
                                    char *key, uint len, int is_path)
{
    int found = 0;
    FileBuffer *aux = list->head;
    while(aux != nullptr){
        LineBuffer *lineBuffer = aux->lineBuffer;
        if(lineBuffer){
            char *ptr = lineBuffer->filePath;
            uint pSize = lineBuffer->filePathSize;
            if(is_path){
                uint k = GetSimplifiedPathName(ptr, pSize);
                ptr = &ptr[k];
                pSize = pSize - k;
            }
            
            if(len == pSize){
                if(StringEqual(ptr, key, len)){
                    *fBuffer = aux;
                    found = 1;
                    break;
                }
            }
        }
        
        aux = aux->next;
    }
    
    return found;
}

void FileBufferList_Init(FileBufferList *list){
    AssertA(list != nullptr, "Invalid file buffers pointer");
    list->count = 0;
    list->head = nullptr;
    list->tail = nullptr;
}

void FileBufferList_Insert(FileBufferList *list, LineBuffer *lBuffer){
    AssertA(list != nullptr && lBuffer != nullptr, "Invalid file buffers pointer");
    FileBuffer *fBuffer = AllocatorGetN(FileBuffer, 1);
    fBuffer->lineBuffer = lBuffer;
    fBuffer->next = nullptr;
    fBuffer->prev = nullptr;
    
    if(list->tail == nullptr || list->count == 0){
        list->head = fBuffer;
        list->tail = fBuffer;
    }else{
        list->tail->next = fBuffer;
        fBuffer->prev = list->tail;
        list->tail = fBuffer;
    }
    
    list->count++;
}

void FileBufferList_Remove(FileBufferList *list, char *name, uint nameLen){
    FileBuffer *fBuffer = nullptr;
    int found = FileBufferList_FindEntry(list, &fBuffer, name, nameLen, 0);
    if(found && fBuffer){
        if(list->head == fBuffer){ // head
            list->head = fBuffer->next;
            if(list->count == 1) list->tail = list->head;
        }else if(list->tail == fBuffer){ // tail
            list->tail = list->tail->prev;
            list->tail->next = nullptr;
        }else{
            fBuffer->prev = fBuffer->next;
            fBuffer->next->prev = fBuffer->prev;
        }
        
        AllocatorFree(fBuffer);
        list->count--;
    }
}

//TODO: This might need some work to detect duplicated files names
int FileBufferList_FindByName(FileBufferList *list, LineBuffer **lineBuffer,
                              char *name, uint nameLen)
{
    AssertA(list != nullptr, "Invalid file buffers pointer");
    int found = 0;
    FileBuffer *aux = list->head;
    while(found == 0 && aux != nullptr){
        if(aux->lineBuffer != nullptr){
            char *ptr = aux->lineBuffer->filePath;
            uint k = GetSimplifiedPathName(ptr, aux->lineBuffer->filePathSize);
            if(aux->lineBuffer->filePathSize - k == nameLen){
                if(StringEqual(&ptr[k], name, nameLen)){
                    if(lineBuffer) *lineBuffer = aux->lineBuffer;
                    found = 1;
                }
            }
        }
        
        aux = aux->next;
    }
    
    return found;
}

int FileBufferList_FindByPath(FileBufferList *list, LineBuffer **lineBuffer,
                              char *path, uint pathLen)
{
    AssertA(list != nullptr, "Invalid file buffers pointer");
    int found = 0;
    FileBuffer *aux = list->head;
    while(found == 0 && aux != nullptr){
        if(aux->lineBuffer != nullptr){
            if(aux->lineBuffer->filePathSize == pathLen){
                if(StringEqual(aux->lineBuffer->filePath, path, pathLen)){
                    if(lineBuffer) *lineBuffer = aux->lineBuffer;
                    found = 1;
                }
            }
        }
        
        aux = aux->next;
    }
    
    return found;
}
