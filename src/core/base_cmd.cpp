#include <view.h>
#include <utilities.h>
#include <string.h>
#include <app.h>

//////////////////////////////////////////////////////////////////////////////////////////
// File opening handling routines
//////////////////////////////////////////////////////////////////////////////////////////
static int ListFileEntriesAndCheckLoaded(char *basePath, FileEntry **entries,
                                         uint *n, uint *size)
{
    if(ListFileEntries(basePath, entries, n, size) < 0){
        return -1;
    }
    
    char path[PATH_MAX];
    FileEntry *arr = *entries;
    for(uint i = 0; i < *n; i++){
        FileEntry *e = &arr[i];
        uint s = snprintf(path, PATH_MAX, "%s%s", basePath, e->path);
        if(AppIsFileLoaded(path, s)){
            e->isLoaded = 1;
        }
    }
    
    return 0;
}

static void SelectableListFreeLineBuffer(View *view){
    LineBuffer *lb = View_SelectableListGetLineBuffer(view);
    LineBuffer_Free(lb);
    AllocatorFree(lb);
}

static void FileOpenUpdateList(View *view){
    FileOpener *opener = View_GetFileOpener(view);
    LineBuffer *lb = View_SelectableListGetLineBuffer(view);
    
    LineBuffer_Free(lb);
    LineBuffer_InitBlank(lb);
    
    for(uint i = 0; i < opener->entryCount; i++){
        FileEntry *e = &opener->entries[i];
        LineBuffer_InsertLine(lb, e->path, e->pLen, 0);
    }
    
    View_SelectableListSwapList(view, lb, 0);
}

/*
* Handles the file search during type, cases to handle:
* - user typed '/' to go forward in a directory;
* - user erased '/' to got backward in a directory;
* - user typed a character to search for a file.
*/
static int FileOpenUpdateEntry(View *view, char *entry, uint len){
    if(len > 0){
        int search = 1;
        FileOpener *opener = View_GetFileOpener(view);
        QueryBar *queryBar = View_GetQueryBar(view);
        char *p = AppGetContextDirectory();
        if(len < opener->pathLen){ // its a character remove update
            // check if we can erase the folder path
            if(opener->basePath[opener->pathLen-1] == '/'){ // change dir
                char tmp = 0;
                uint n = GetSimplifiedPathName(opener->basePath, opener->pathLen-1);
                
                tmp = opener->basePath[n];
                opener->basePath[n] = 0;
                if(CHDIR(opener->basePath) < 0){
                    printf("Failed to change to %s directory\n", opener->basePath);
                    opener->basePath[n] = tmp;
                    return 0;
                }
                
                if(ListFileEntriesAndCheckLoaded(opener->basePath, &opener->entries,
                                                 &opener->entryCount,
                                                 &opener->entrySize) < 0)
                {
                    printf("Failed to list files from %s\n", opener->basePath);
                    opener->basePath[n] = tmp;
                    IGNORE(CHDIR(opener->basePath));
                    return 0;
                }
                
                opener->pathLen = n;
                p[n] = 0; // copy opener->basePath into context cwd
                
                QueryBar_SetEntry(queryBar, view, opener->basePath, n);
                FileOpenUpdateList(view);
                search = 0;
            }
        }else if(entry[len-1] == '/'){ // user typed '/'
            char *content = nullptr;
            uint contentLen = 0;
            QueryBar_GetWrittenContent(queryBar, &content, &contentLen);
            
            if(CHDIR(content) < 0){
                printf("Failed to change to %s directory\n", content);
                return -1;
            }
            
            if(ListFileEntriesAndCheckLoaded(content, &opener->entries,
                                             &opener->entryCount, &opener->entrySize) < 0)
            {
                printf("Failed to list files from %s\n", content);
                IGNORE(CHDIR(opener->basePath));
                return -1;
            }
            
            opener->pathLen = len;
            Memset(p, 0x00, PATH_MAX);
            Memcpy(p, entry, len);
            Memcpy(opener->basePath, entry, len);
            
            FileOpenUpdateList(view);
            search = 0;
        }
        
        if(search){
            // text has changed, update the search
            char *content = nullptr;
            uint contentLen = 0;
            QueryBar_GetWrittenContent(queryBar, &content, &contentLen);
            uint s = GetSimplifiedPathName(content, contentLen);
            if(contentLen > s)
                View_SelectableListFilterBy(view, &content[s], contentLen-s);
            else
                View_SelectableListFilterBy(view, nullptr, 0);
        }
    }else{
        View_SelectableListFilterBy(view, nullptr, 0);
    }
    
    return 1;
}

int FileOpenCommandEntry(QueryBar *queryBar, View *view){
    char *content = nullptr;
    uint contentLen = 0;
    QueryBar_GetWrittenContent(queryBar, &content, &contentLen);
    return FileOpenUpdateEntry(view, content, contentLen);
}

int FileOpenCommandCancel(QueryBar *queryBar, View *view){
    FileOpener *opener = View_GetFileOpener(view);
    SelectableListFreeLineBuffer(view);
    AllocatorFree(opener->entries);
    opener->entries = nullptr;
    opener->entryCount = 0;
    opener->entrySize = 0;
    return 1;
}

int FileOpenCommandCommit(QueryBar *queryBar, View *view){
    uint rindex = 0;
    Buffer *buffer = nullptr;
    FileEntry *entry = nullptr;
    FileOpener *opener = View_GetFileOpener(view);
    int active = View_SelectableListGetActiveIndex(view);
    if(active == -1){ // is a creation
        queryBar->fileOpenCallback(view, entry);
        SelectableListFreeLineBuffer(view);
        return 1;
    }
    
    View_SelectableListGetItem(view, active, &buffer);
    if(!buffer){
        SelectableListFreeLineBuffer(view);
        return -1;
    }
    
    rindex = View_SelectableListGetRealIndex(view, active);
    entry = &opener->entries[rindex];
    
    if(entry->type == DescriptorDirectory){
        char *p = AppGetContextDirectory();
        char *folder = opener->basePath;
        Memcpy(&folder[opener->pathLen], entry->path, entry->pLen);
        folder[opener->pathLen + entry->pLen] = '/';
        folder[opener->pathLen + entry->pLen + 1] = 0;
        opener->pathLen += entry->pLen + 1;
        if(CHDIR(opener->basePath) < 0){
            printf("Failed to change to %s directory\n", opener->basePath);
            return 1;
        }
        
        if(ListFileEntriesAndCheckLoaded(opener->basePath, &opener->entries,
                                         &opener->entryCount, &opener->entrySize) < 0)
        {
            printf("Failed to list files from %s\n", opener->basePath);
            IGNORE(CHDIR(opener->basePath));
            return 1;
        }
        
        Memset(p, 0x00, PATH_MAX);
        opener->basePath[opener->pathLen] = 0;
        
        Memcpy(p, opener->basePath, opener->pathLen);
        QueryBar_SetEntry(queryBar, view, opener->basePath, opener->pathLen);
        FileOpenUpdateList(view);
        return 0;
    }
    
    queryBar->fileOpenCallback(view, entry);
    
    SelectableListFreeLineBuffer(view);
    AllocatorFree(opener->entries);
    opener->entries = nullptr;
    opener->entryCount = 0;
    opener->entrySize = 0;
    return 1;
}

int FileOpenerCommandStart(View *view, char *basePath, ushort len,
                           OnFileOpenCallback onOpenFile)
{
    AssertA(view != nullptr && basePath != nullptr && len > 0,
            "Invalid setup of file opener");
    
    LineBuffer *lineBuffer = nullptr;
    char *pEntry = nullptr;
    const char *header = "Open File";
    uint hlen = strlen(header);
    ushort length = Min(len, PATH_MAX-1);
    QueryBar *queryBar = View_GetQueryBar(view);
    FileOpener *opener = View_GetFileOpener(view);
    
    Memcpy(opener->basePath, basePath, length);
    opener->basePath[length] = 0;
    opener->pathLen = length;
    opener->entryCount = 0;
    
    if(ListFileEntriesAndCheckLoaded(basePath, &opener->entries,
                                     &opener->entryCount, &opener->entrySize) < 0)
    {
        printf("Failed to list files from %s\n", basePath);
        return -1;
    }
    
    pEntry = AllocatorGetN(char, len+2);
    lineBuffer = AllocatorGetN(LineBuffer, 1);
    if(basePath[len-1] != '/'){
        snprintf(pEntry, len+2, "%s/", basePath);
        opener->basePath[length] = '/';
        opener->basePath[length+1] = 0;
        opener->pathLen++;
    }else{
        snprintf(pEntry, len+2, "%s", basePath);
        len -= 1;
    }
    
    LineBuffer_InitBlank(lineBuffer);
    
    for(uint i = 0; i < opener->entryCount; i++){
        FileEntry *e = &opener->entries[i];
        LineBuffer_InsertLine(lineBuffer, e->path, e->pLen, 0);
    }
    
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           FileOpenCommandEntry, FileOpenCommandCancel,
                           FileOpenCommandCommit, nullptr);
    
    QueryBar_SetEntry(queryBar, view, pEntry, len+1);
    queryBar->fileOpenCallback = onOpenFile;
    AllocatorFree(pEntry);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Switch buffer routines.
//////////////////////////////////////////////////////////////////////////////////////////
static int SwitchBuffersUpdateEntry(View *view, char *entry, uint len){
    if(len > 0){
        View_SelectableListFilterBy(view, entry, len);
    }else{
        View_SelectableListFilterBy(view, nullptr, 0);
    }
    
    return 1;
}

int SwitchBufferCommandCancel(QueryBar *queryBar, View *view){
    SelectableListFreeLineBuffer(view);
    return 1;
}

int SwitchBufferCommandCommit(QueryBar *queryBar, View *view){
    LineBuffer *lineBuffer = nullptr;
    Buffer *buffer = nullptr;
    
    FileBufferList *fList = AppGetFileBufferList();
    BufferView *bView = View_GetBufferView(view);
    
    uint active = View_SelectableListGetActiveIndex(view);
    View_SelectableListGetItem(view, active, &buffer);
    if(!buffer){
        return 0;
    }
    
    if(FileBufferList_FindByName(fList, &lineBuffer, buffer->data,
                                 buffer->taken) == 0)
    {
        return 0;
    }
    
    BufferView_SwapBuffer(bView, lineBuffer);
    SelectableListFreeLineBuffer(view);
    return 1;
}

int SwitchBufferCommandEntry(QueryBar *queryBar, View *view){
    char *content = nullptr;
    uint contentLen = 0;
    QueryBar_GetWrittenContent(queryBar, &content, &contentLen);
    return SwitchBuffersUpdateEntry(view, content, contentLen);
}

int SwitchBufferCommandStart(View *view){
    AssertA(view != nullptr, "Invalid view pointer");
    LineBuffer *lineBuffer = nullptr;
    List<FileBuffer> *list = nullptr;
    const char *header = "Switch Buffer";
    uint hlen = strlen(header);
    FileBufferList *fBuffers = AppGetFileBufferList();
    
    lineBuffer = AllocatorGetN(LineBuffer, 1);
    LineBuffer_InitBlank(lineBuffer);
    
    list = fBuffers->fList;
    
    auto filler = [&](FileBuffer *buf) -> int{
        if(buf){
            if(buf->lineBuffer){
                char *ptr = buf->lineBuffer->filePath;
                uint len = buf->lineBuffer->filePathSize;
                uint n = GetSimplifiedPathName(ptr, len);
                LineBuffer_InsertLine(lineBuffer, &ptr[n], len - n, 0);
            }
        }
        
        return 1;
    };
    
    List_Transverse<FileBuffer>(list, filler);
    
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SwitchBufferCommandEntry, SwitchBufferCommandCancel,
                           SwitchBufferCommandCommit, nullptr);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
int QueryBarCommandSearch(QueryBar *queryBar, LineBuffer *lineBuffer,
                          DoubleCursor *dcursor)
{
    AssertA(queryBar != nullptr && lineBuffer != nullptr && dcursor != nullptr,
            "Invalid search input");
    AssertA(queryBar->cmd == QUERY_BAR_CMD_SEARCH ||
            queryBar->cmd == QUERY_BAR_CMD_REVERSE_SEARCH,
            "QueryBar is in incorrect state");
    
    char *str = nullptr;
    uint slen = 0;
    QueryBarCmdSearch *searchResult = &queryBar->searchCmd;
    QueryBarCommand id = queryBar->cmd;
    
    QueryBar_GetWrittenContent(queryBar, &str, &slen);
    if(slen > 0){
        /*
    * We cannot use token comparation because we would be unable to
* capture compositions
*/
        
        int done = 0;
        vec2ui cursor = dcursor->textPosition;
        uint line = cursor.x;
        uint start = cursor.y;
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, line);
        do{
            /* Grab buffer to search */
            if(buffer){
                if(buffer->taken > 0){
                    int at = 0;
                    if(id == QUERY_BAR_CMD_SEARCH){
                        at = StringSearch(str, &buffer->data[start], slen, 
                                          buffer->taken - start);
                        if(at >= 0) at += start;
                    }else{
                        at = ReverseStringSearch(str, buffer->data, slen, start);
                    }
                    
                    if(at >= 0){ // found
                        searchResult->lineNo = line;
                        searchResult->position = at;
                        searchResult->length = slen;
                        searchResult->valid = 1;
                        //printf("[%u - %u]\n", searchResult->lineNo, searchResult->position);
                        return 1;
                    }
                }
            }
            
            if(id == QUERY_BAR_CMD_SEARCH && line < lineBuffer->lineCount){
                line++;
                start = 0;
                buffer = LineBuffer_GetBufferAt(lineBuffer, line);
            }else if(id == QUERY_BAR_CMD_REVERSE_SEARCH && line > 0){
                line--;
                buffer = LineBuffer_GetBufferAt(lineBuffer, line);
                start = buffer->taken-1;
            }else{
                done = 1;
            }
        }while(done == 0);
    }
    
    printf("Invalid\n");
    searchResult->valid = 0;
    return 0;
}
