/* date = March 3rd 2021 6:16 pm */

#ifndef FILE_BUFFER_H
#define FILE_BUFFER_H
#include <buffers.h>
#include <utilities.h>

/*
* FileBuffers are storage reserved to maintain LineBuffers opened but
* not necessarily in use. Helper structure for the App handling of things.
* Basic a list.
*/
typedef struct file_buffer_t{
    LineBuffer *lineBuffer;
}FileBuffer;

typedef struct{
    List<FileBuffer> *fList;
}FileBufferList;

/*
* Initializes a file buffer list.
*/
void FileBufferList_Init(FileBufferList *list);

/*
* Inserts a new file buffer into a list given in 'list'.
*/
void FileBufferList_Insert(FileBufferList *list, LineBuffer *lBuffer);

/*
* Locates a buffer by its path.
*/
int FileBufferList_FindByPath(FileBufferList *list, LineBuffer **lineBuffer,
                              char *path, uint pathLen);

/*
* Locates a buffer by its name.
*/
int FileBufferList_FindByName(FileBufferList *list, LineBuffer **lineBuffer,
                              char *name, uint nameLen);

/*
* Removes an entry of the list given its name.
*/
void FileBufferList_Remove(FileBufferList *list, char *name, uint nameLen);

/* Debug stuff */
void FileBufferList_DebugList(FileBufferList *list);

#endif //FILE_BUFFER_H
