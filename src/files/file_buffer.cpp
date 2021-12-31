#include <file_buffer.h>
#include <utilities.h>
#include <types.h>

static int FileBufferList_FindEntry(FileBufferList *list, FileBuffer **fBuffer,
                                    char *key, uint keylen, int is_path)
{
    int found = 0;
    FileBuffer *tmp = nullptr;
    auto locate = [&](FileBuffer *buf) -> int{
        int rv = 0;
        if(buf){
            if(buf->lineBuffer){
                char *ptr = buf->lineBuffer->filePath;
                uint len = buf->lineBuffer->filePathSize;
                if(is_path == 0){
                    uint k = GetSimplifiedPathName(ptr, len);
                    len = len - k;
                    ptr = &buf->lineBuffer->filePath[k];
                }

                if(len == keylen){
                    rv = StringEqual(ptr, key, len);
                }
            }
        }

        return rv;
    };

    tmp = List_Find<FileBuffer>(list->fList, locate);
    if(tmp){
        *fBuffer = tmp;
        found = 1;
    }
    return found;
}

void FileBufferList_Init(FileBufferList *list){
    AssertA(list != nullptr, "Invalid file buffers pointer");
    list->fList = List_Create<FileBuffer>();
}

void FileBufferList_Insert(FileBufferList *list, LineBuffer *lBuffer){
    AssertA(list != nullptr && lBuffer != nullptr, "Invalid file buffers pointer");
    FileBuffer fBuffer = {
        .lineBuffer = lBuffer,
    };

    List_Push<FileBuffer>(list->fList, &fBuffer);
}

void FileBufferList_Remove(FileBufferList *list, LineBuffer *lineBuffer){
    auto finder = [&](FileBuffer *buf) -> int{
        int rv = 0;
        if(buf){
            rv = buf->lineBuffer == lineBuffer ? 1 : 0;
        }

        return rv;
    };

    List_Erase<FileBuffer>(list->fList, finder);
}

void FileBufferList_Remove(FileBufferList *list, char *name, uint nameLen){
    auto finder = [&](FileBuffer *buf) -> int{
        int rv = 0;
        if(buf){
            if(buf->lineBuffer){
                char *ptr = buf->lineBuffer->filePath;
                uint len = buf->lineBuffer->filePathSize;
                if(len == nameLen){
                    rv = StringEqual(ptr, name, nameLen);
                }
            }
        }

        return rv;
    };

    List_Erase<FileBuffer>(list->fList, finder);
}

//TODO: This might need some work to detect duplicated files names
int FileBufferList_FindByName(FileBufferList *list, LineBuffer **lineBuffer,
                              char *name, uint nameLen)
{
    AssertA(list != nullptr, "Invalid file buffers pointer");
    FileBuffer *fBuffer = nullptr;
    int found = 0;
    if(FileBufferList_FindEntry(list, &fBuffer, name, nameLen, 0)){
        AssertA(fBuffer != nullptr, "Invalid result on get entry");
        if(lineBuffer) *lineBuffer = fBuffer->lineBuffer;
        found = 1;
    }

    return found;
}

int FileBufferList_FindByPath(FileBufferList *list, LineBuffer **lineBuffer,
                              char *path, uint pathLen)
{
    AssertA(list != nullptr, "Invalid file buffers pointer");
    FileBuffer *fBuffer = nullptr;
    int found = 0;
    if(FileBufferList_FindEntry(list, &fBuffer, path, pathLen, 1)){
        AssertA(fBuffer != nullptr, "Invalid result on get entry");
        if(lineBuffer) *lineBuffer = fBuffer->lineBuffer;
        found = 1;
    }

    return found;
}
