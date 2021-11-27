#include <view.h>
#include <utilities.h>
#include <string.h>
#include <app.h>
#include <file_provider.h>
#include <parallel.h>
#include <bufferview.h>
#include <sstream>
#include <map>

static std::map<std::string, std::string> aliasMap;
static std::string envDir;
static std::map<std::string, std::string> mathSymbolMap;

static void InitializeMathSymbolList(){
    static bool is_math_symbol_inited = false;
    if(!is_math_symbol_inited){
        mathSymbolMap["pi"] = "π";    mathSymbolMap["Pi"] = "Π";
        mathSymbolMap["theta"] = "θ"; mathSymbolMap["Theta"] = "Θ";
        mathSymbolMap["mu"] = "μ";    mathSymbolMap["Mu"] = "M";
        mathSymbolMap["zeta"] = "ζ";  mathSymbolMap["Zeta"] = "Z";
        mathSymbolMap["eta"] = "η";   mathSymbolMap["Eta"] = "H";
        mathSymbolMap["iota"] = "ι";  mathSymbolMap["Iota"] = "I";
        mathSymbolMap["kappa"] = "κ"; mathSymbolMap["Kappa"] = "K";
        mathSymbolMap["lambda"] = "λ";mathSymbolMap["Lambda"] = "Λ";
        mathSymbolMap["nu"] = "ν";    mathSymbolMap["Nu"] = "N";
        mathSymbolMap["xi"] = "ξ";    mathSymbolMap["Xi"] = "Ξ";
        mathSymbolMap["rho"] = "ρ";   mathSymbolMap["Rho"] = "P";
        mathSymbolMap["sigma"] = "σ"; mathSymbolMap["Sigma"] = "Σ";
        mathSymbolMap["tau"] = "τ";   mathSymbolMap["Tau"] = "T";
        mathSymbolMap["phi"] = "φ";   mathSymbolMap["Phi"] = "Φ";
        mathSymbolMap["chi"] = "χ";   mathSymbolMap["Chi"] = "X";
        mathSymbolMap["psi"] = "ψ";   mathSymbolMap["Psi"] = "Ψ";
        mathSymbolMap["omega"] = "ω"; mathSymbolMap["Omega"] = "Ω";
        mathSymbolMap["beta"] = "β";  mathSymbolMap["Beta"] = "B";
        mathSymbolMap["alpha"] = "α"; mathSymbolMap["Alpha"] = "A";
        mathSymbolMap["gamma"] = "γ"; mathSymbolMap["Gamma"] = "Γ";
        mathSymbolMap["delta"] = "δ"; mathSymbolMap["Delta"] = "Δ";
        mathSymbolMap["epsilon"] = "ε"; mathSymbolMap["Epsilon"] = "E";
        mathSymbolMap["partial"] = "∂"; mathSymbolMap["int"] = "∫";
        mathSymbolMap["inf"] = "∞";
        is_math_symbol_inited = true;
    }
}

/* Default helper functions */
static void SelectableListFreeLineBuffer(View *view){
    LineBuffer *lb = View_SelectableListGetLineBuffer(view);
    LineBuffer_Free(lb);
    AllocatorFree(lb);
}

static int SelectableListDynamicEntry(View *view, char *entry, uint len){
    if(len > 0){
        View_SelectableListFilterBy(view, entry, len);
    }else{
        View_SelectableListFilterBy(view, nullptr, 0);
    }

    return 1;
}

static int SelectableListDefaultEntry(QueryBar *queryBar, View *view){
    char *content = nullptr;
    uint contentLen = 0;
    QueryBar_GetWrittenContent(queryBar, &content, &contentLen);
    return SelectableListDynamicEntry(view, content, contentLen);
}

static int SelectableListDefaultCancel(QueryBar *queryBar, View *view){
    SelectableListFreeLineBuffer(view);
    return 1;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Interpretation commands
//////////////////////////////////////////////////////////////////////////////////////////
int BaseCommand_CreateCodyFolderIfNeeded(){
    int rv = -1;
    std::string dir = AppGetConfigDirectory();
    FileEntry entry;
    char folder[PATH_MAX];
    int r = GuessFileEntry((char *)dir.c_str(), dir.size(), &entry, folder);

    if(r < 0){
        //TODO: mkdir
        rv = mkdir(dir.c_str(), 0777);
    }else if(entry.type == DescriptorDirectory){
        // already created
        rv = 0;
    }else{
        printf("Cannot create cody folder, would corrupt filesystem\n");
    }

    return rv;
}

int BaseCommand_AddFileEntryIntoAutoLoader(char *entry, uint size){
    if(BaseCommand_CreateCodyFolderIfNeeded() < 0){
        return -1;
    }

    if(size > 0){
        std::string path = AppGetConfigFilePath();
        FILE *fp = fopen(path.c_str(), "a+");
        if(fp){
            fprintf(fp, "%s\n", entry);
            fclose(fp);
        }
    }

    return 0;
}

struct QueriableSearchResult{
    GlobalSearchResult res[MAX_SEARCH_ENTRIES * MAX_THREADS];
    int count;
};

GlobalSearch results[MAX_THREADS];
QueriableSearchResult linearResults;

uint BaseCommand_FetchGlobalSearchData(GlobalSearch **gSearch){
    if(gSearch){
        *gSearch = &results[0];
        return MAX_THREADS;
    }else{
        return 0;
    }
}

void BaseCommand_JumpViewToBuffer(View *view, LineBuffer *lineBuffer, vec2i p){
    BufferView *bView = View_GetBufferView(view);
    BufferView_SwapBuffer(bView, lineBuffer, CodeView); // is this always CodeView?
    BufferView_CursorToPosition(bView, p.x, p.y);
    BufferView_GhostCursorFollow(bView);
}

int GlobalSearchCommandCommit(QueryBar *queryBar, View *view){
    int active = View_SelectableListGetActiveIndex(view);
    if(active == -1){
        return 0;
    }

    GlobalSearchResult *result = &linearResults.res[active];
    LineBuffer *targetLineBuffer = result->lineBuffer;
#if 0
    Buffer *buffer = LineBuffer_GetBufferAt(targetLineBuffer, result->line);
    printf("Picked %s:%d:%d : %s\n", targetLineBuffer->filePath,
           result->line, result->col, buffer->data);
#endif

    BaseCommand_JumpViewToBuffer(view, targetLineBuffer,
                                 vec2i(result->line, result->col));

    SelectableListFreeLineBuffer(view);
    return 1;
}

int SearchAllFilesCommandStart(View *view){
    AssertA(view != nullptr, "Invalid view pointer");
    const char *header = "Search Files";
    uint hlen = strlen(header);
    LineBuffer *lineBuffer = AllocatorGetN(LineBuffer, 1);
    LineBuffer_InitBlank(lineBuffer);
    std::string root = AppGetRootDirectory();

    char *rootStr = (char *)root.c_str();
    uint rootLen = root.size();

    linearResults.count = 0;
    uint max_written_len = 60;
    for(int tid = 0; tid < MAX_THREADS; tid++){
        GlobalSearch *res = &results[tid];
        for(uint i = 0; i < res->count; i++){
            if(!res->results[i].lineBuffer) continue;
            char *p = (char *)res->results[i].lineBuffer->filePath;
            uint l = res->results[i].lineBuffer->filePathSize;
            uint at = 0;
            int insert = 1;
            for(int k = 0; k < linearResults.count; k++){
                GlobalSearchResult *r = &linearResults.res[k];
                if(r->lineBuffer == res->results[i].lineBuffer &&
                   r->line == res->results[i].line)
                {
                    insert = 0; break;
                }
            }

            if(!insert) continue;

            if(StringStartsWith(p, l, rootStr, rootLen)){
                at = rootLen;
                while(at < l && p[at] == '/') at ++;
            }

            LineBuffer *lBuffer = res->results[i].lineBuffer;
            char *pptr = &(res->results[i].lineBuffer->filePath[at]);
            char m[256];

            Buffer *buffer = LineBuffer_GetBufferAt(lBuffer, res->results[i].line);
            uint f = buffer->taken;
            uint start_loc = res->results[i].col;
            uint end_loc = 0;
            uint len = 0;

            if(start_loc > max_written_len * 0.4){
                start_loc = start_loc - max_written_len * 0.4;
            }else{
                start_loc = 0;
            }

            uint sid = Buffer_Utf8RawPositionToPosition(buffer, start_loc);
            uint tid = Buffer_GetTokenAt(buffer, sid);
            uint fid = Buffer_FindFirstNonEmptyToken(buffer);
            uint pickId = 0;
            for(pickId = tid; pickId < buffer->tokenCount; pickId++){
                if(buffer->tokens[pickId].identifier != TOKEN_ID_SPACE){
                    break;
                }
            }

            start_loc = buffer->tokens[pickId].position;

            end_loc = f - start_loc > max_written_len ? start_loc + max_written_len : f;
            char ss = buffer->data[end_loc];
            buffer->data[end_loc] = 0;

            if(start_loc == 0 || pickId == fid){
                len = snprintf(m, sizeof(m), "%s:%d: ", pptr, res->results[i].line);
            }else{
                len = snprintf(m, sizeof(m), "%s:%d:... ", pptr, res->results[i].line);
            }

            if(end_loc == f){
                len += snprintf(&m[len], sizeof(m)-len, "%s", &buffer->data[start_loc]);
            }else{
                len += snprintf(&m[len], sizeof(m)-len, "%s ...", &buffer->data[start_loc]);
            }

            //printf("Adding %s\n", m);

            buffer->data[end_loc] = ss;

            linearResults.res[linearResults.count++] = res->results[i];

            LineBuffer_InsertLine(lineBuffer, (char *)m, len, 0);
        }
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    filter.allowFreeType = 0;
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           GlobalSearchCommandCommit, &filter);
    return 0;
}

int BaseCommand_InsertMappedSymbol(char *cmd, uint size){
    int r = 0;
    if(cmd[0] == '\\'){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[1], size - 1);
        if(e < 0) return r;
        e += 1;

        std::string symname(&cmd[e]);
        InitializeMathSymbolList();

        if(mathSymbolMap.find(symname) != mathSymbolMap.end()){
            std::string value = mathSymbolMap[symname];
            AppPasteString(value.c_str(), value.size());
        }
    }

    return r;
}

void BaseCommand_GitDiff(char *cmd, uint size){
    uint len = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(arg == nullptr){
        AppCommandGitDiffCurrent();
    }
}

int BaseCommand_Git(char *cmd, uint size){
    int r = 0;
    const char *git = "git ";
    const int gitlen = 4;
    if(StringStartsWith(cmd, size, (char *)git, gitlen)){
        r = 1;
        uint len = 0;
        char *gitCmd = StringNextWord(cmd, size, &len);
        if(StringEqual(gitCmd, (char *)"diff", Min(len, 4))){
            if(len != 4) return r; // check for ill construted string
            BaseCommand_GitDiff(gitCmd, size-len);
        }else if(StringEqual(gitCmd, (char *)"status", Min(len, 6))){
            AppCommandGitStatus();
        }
    }

    return r;
}

int BaseCommand_SearchAllFiles(char *cmd, uint size){
    int r = 0;
    std::string search("search ");
    if(StringStartsWith(cmd, size, (char *)search.c_str(), search.size())){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[search.size()], size - search.size());
        if(e < 0) return r;
        e += search.size();

        std::string searchStr(&cmd[e]);

        FileBufferList *bufferList = FileProvider_GetBufferList();
        FileBuffer *bufferArray[MAX_QUERIABLE_BUFFERS];
        uint size = List_FetchAsArray(bufferList->fList, bufferArray,
                                      MAX_QUERIABLE_BUFFERS, 0);

        for(int i = 0; i < MAX_THREADS; i++) results[i].count = 0;

        const char *strPtr = searchStr.c_str();
        uint stringLen = searchStr.size();

        ParallelFor("String Seach", 0, size, [&](int i, int tid){
            LineBuffer *lineBuffer = bufferArray[i]->lineBuffer;
            //printf("[%d] ==> %s\n", tid, lineBuffer->filePath);
            if(tid > MAX_THREADS-1){
                printf("Ooops cannot query, too many threads\n");
                return;
            }

            GlobalSearch *threadResult = &results[tid];
            if(threadResult->count >= MAX_SEARCH_ENTRIES-1){
                //printf("Too many results\n");
                return;
            }

            for(uint i = 0; i < lineBuffer->lineCount; i++){
                Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
                if(buffer->taken > 0){
                    int at = 0;
                    uint start = 0;
                    do{
                        at = StringSearch((char *)strPtr, &buffer->data[start],
                                          stringLen, buffer->taken - start);
                        if(at >= 0){
                            uint loc = threadResult->count;
                            if(loc >= MAX_SEARCH_ENTRIES-1){
                                //printf("Too many results\n");
                                return;
                            }

                            threadResult->results[loc].line = i;
                            threadResult->results[loc].col = at + start;
                            threadResult->results[loc].lineBuffer = lineBuffer;
                            start += at + stringLen;
                            threadResult->count++;
                        }
                    }while(at >= 0);
                }
            }
        });
#if 0
        for(int tid = 0; tid < MAX_THREADS; tid++){
            GlobalSearch *threadResult = &results[tid];
            for(uint i = 0; i < threadResult->count; i++){
                printf("[%s] = line (%d) col (%d)\n",
                        threadResult->results[i].lineBuffer->filePath,
                        threadResult->results[i].line,
                        threadResult->results[i].col);
            }
        }
#endif

        View *view = AppGetActiveView();
        if(SearchAllFilesCommandStart(view) >= 0){
            AppSetBindingsForState(View_SelectableList);
            r = 2;
        }
    }

    return r;
}

int BaseCommand_Interpret_AliasInsert(char *cmd, uint size, View *view){
    int r = 0;
    std::string alias("alias ");
    if(StringStartsWith(cmd, size, (char *)alias.c_str(), alias.size())){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[alias.size()], size - alias.size());
        if(e < 0) return r;
        e += alias.size();

        char *target = &cmd[e];

        int p = StringFindFirstChar(target, size - e, '=');
        if(p < 0) return r;

        int n = p-1;
        // rewind and remove space
        for(int f = p-1; f > e; f--){
            if(cmd[f] == ' ') n--;
            else break;
        }

        char t = target[n];
        target[n] = 0;

        std::string targetStr(target);
        target[n] = t;

        int s = StringFirstNonEmpty(&target[p+1], size - e - p - 1);
        if(s < 0) return r;

        char *value = &target[p+1+s];

        char vv = cmd[size];
        cmd[size] = 0;
        std::string valueStr(value);
        cmd[size] = vv;

        aliasMap[targetStr] = valueStr;
        printf("Alias [%s] = [%s]\n", targetStr.c_str(), valueStr.c_str());
    }

    return r;
}

std::string BaseCommand_GetBasePath(){
    if(envDir.size() > 0) return std::string("/") + envDir;
    return "/build";
}

int BaseCommand_SetExecPath(char *cmd, uint size){
    int r = 0;
    // TODO: options?
    std::string senv("set-env ");
    if(StringStartsWith(cmd, size, (char *)senv.c_str(), senv.size())){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[senv.size()], size - senv.size());
        if(e < 0) return r;
        char v = cmd[size];
        cmd[size] = 0;

        envDir = std::string(&cmd[senv.size() + e]);
        cmd[size] = v;
    }

    return r;
}

std::string BaseCommand_Interpret_Alias(char *cmd, uint size){
    std::string strCmd(cmd);
    if(aliasMap.find(strCmd) != aliasMap.end()){
        strCmd = aliasMap[strCmd];
    }

    return strCmd;
}

int BaseCommand_Interpret(char *cmd, uint size, View *view){
    // TODO: map of commands? maybe set some function pointers to this
    // TODO: create a standard for this, a json or at least something like bubbles
    int rv = -1;
    if(BaseCommand_Interpret_AliasInsert(cmd, size, view)){
        return 0;
    }else if(BaseCommand_SetExecPath(cmd, size)){
        return 0;
    }else if(BaseCommand_InsertMappedSymbol(cmd, size)){
        return 0;
    }else{
        rv = BaseCommand_SearchAllFiles(cmd, size);
        if(rv != 0) return rv;

        rv = BaseCommand_Git(cmd, size);
        if(rv != 0) return rv;

        rv = -1;
    }

    std::string strCmd = BaseCommand_Interpret_Alias(cmd, size);

    std::string rootDir = AppGetRootDirectory() + BaseCommand_GetBasePath();
    std::stringstream ss;
    ss << "cd " << rootDir << " && " << strCmd;

    std::string md = ss.str();
    LockedLineBuffer *lockedBuffer = nullptr;
    GetExecutorLockedLineBuffer(&lockedBuffer);
    ViewNode *vnode = AppGetNextViewNode();
    BufferView *bView = View_GetBufferView(view);
    if(vnode){
        if(vnode->view){
            bView = View_GetBufferView(vnode->view);
        }
    }

    BufferView_SwapBuffer(bView, lockedBuffer->lineBuffer, BuildView);
    // because the parallel thread will reset the linebuffer we need
    // to make sure the cursor is located in a valid range for rendering
    // otherwise in case the build buffer does is updated too fast it can
    // generate a SIGSEGV. This needs to run after the swap as we need
    // to make sure the position cache map is updated with whatever
    // is being rendered at the moment.
    BufferView_CursorToPosition(bView, 0, 0);
    BufferView_GhostCursorFollow(bView);
    // TODO: function LockedBufferStartRender() or something
    lockedBuffer->render_state = 0;
    ExecuteCommand(md);

    return rv;
}

//////////////////////////////////////////////////////////////////////////////////////////
// File opening handling routines
//////////////////////////////////////////////////////////////////////////////////////////
static int ListFileEntriesAndCheckLoaded(char *basePath, FileEntry **entries,
                                         uint *n, uint *size)
{
    std::string refPath(basePath);
    if(refPath[refPath.size()-1] != '/'){
        refPath += "/";
    }

    if(ListFileEntries((char *)refPath.c_str(), entries, n, size) < 0){
        return -1;
    }
    
    char path[PATH_MAX];
    FileEntry *arr = *entries;
    for(uint i = 0; i < *n; i++){
        FileEntry *e = &arr[i];
        uint s = snprintf(path, PATH_MAX, "%s%s", refPath.c_str(), e->path);
        if(FileProvider_IsFileOpened(path, s)){
            e->isLoaded = 1;
        }
    }
    
    return 0;
}

static void FileOpenUpdateList(View *view){
    FileOpener *opener = View_GetFileOpener(view);
    LineBuffer *lb = View_SelectableListGetLineBuffer(view);

    // TODO: This is very ineficient, but it is working so...
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
    int rv = 1;
    if(active == -1){ // is a creation
        queryBar->fileOpenCallback(view, entry);
        goto end;
    }
    
    View_SelectableListGetItem(view, active, &buffer);
    if(!buffer){
        rv = -1;
        goto end;
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
            goto end;
        }
        
        if(ListFileEntriesAndCheckLoaded(opener->basePath, &opener->entries,
                                         &opener->entryCount, &opener->entrySize) < 0)
        {
            printf("Failed to list files from %s\n", opener->basePath);
            IGNORE(CHDIR(opener->basePath));
            goto end;
        }
        
        Memset(p, 0x00, PATH_MAX);
        opener->basePath[opener->pathLen] = 0;
        
        Memcpy(p, opener->basePath, opener->pathLen);
        QueryBar_SetEntry(queryBar, view, opener->basePath, opener->pathLen);
        FileOpenUpdateList(view);
        return 0;
    }
    
    queryBar->fileOpenCallback(view, entry);
end:
    SelectableListFreeLineBuffer(view);
    AllocatorFree(opener->entries);
    opener->entries = nullptr;
    opener->entryCount = 0;
    opener->entrySize = 0;
    return rv;
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
int SwitchBufferCommandCommit(QueryBar *queryBar, View *view){
    LineBuffer *lineBuffer = nullptr;
    Buffer *buffer = nullptr;
    
    FileBufferList *fList = FileProvider_GetBufferList();
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
    
    BufferView_SwapBuffer(bView, lineBuffer, CodeView);
    SelectableListFreeLineBuffer(view);
    return 1;
}

int SwitchBufferCommandStart(View *view){
    AssertA(view != nullptr, "Invalid view pointer");
    LineBuffer *lineBuffer = nullptr;
    List<FileBuffer> *list = nullptr;
    const char *header = "Switch Buffer";
    uint hlen = strlen(header);
    FileBufferList *fBuffers = FileProvider_GetBufferList();
    
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
    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    filter.allowFreeType = 0;

    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           SwitchBufferCommandCommit, &filter);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Switch theme routines.
//////////////////////////////////////////////////////////////////////////////////////////
int SwitchThemeCommandCommit(QueryBar *queryBar, View *view){
    Buffer *buffer = nullptr;
    uint active = View_SelectableListGetActiveIndex(view);
    View_SelectableListGetItem(view, active, &buffer);
    if(!buffer){
        return 0;
    }

    SwapDefaultTheme(buffer->data, buffer->taken);

    SelectableListFreeLineBuffer(view);
    return 1;    
}

int SwitchThemeCommandStart(View *view){
    AssertA(view != nullptr, "Invalid view pointer");
    std::vector<ThemeDescription> *themes = nullptr; 
    LineBuffer *lineBuffer = nullptr;
    const char *header = "Switch Theme";
    uint hlen = strlen(header);
    lineBuffer = AllocatorGetN(LineBuffer, 1);

    GetThemeList(&themes);
    AssertA(themes != nullptr, "Invalid theme list");

    LineBuffer_InitBlank(lineBuffer);
    for(uint i = 0; i < themes->size(); i++){
        ThemeDescription dsc = themes->at(i);
        uint slen = strlen(dsc.name);
        LineBuffer_InsertLine(lineBuffer, (char *)dsc.name, slen, 0);
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    filter.allowFreeType = 0;
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           SwitchThemeCommandCommit, &filter);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Seach text command routines.
//////////////////////////////////////////////////////////////////////////////////////////
int QueryBarCommandSearch(QueryBar *queryBar, LineBuffer *lineBuffer,
                          DoubleCursor *dcursor, char *toSearch,
                          uint toSearchLen)
{
    AssertA(queryBar != nullptr && lineBuffer != nullptr && dcursor != nullptr,
            "Invalid search input");
    AssertA(queryBar->cmd == QUERY_BAR_CMD_SEARCH ||
            queryBar->cmd == QUERY_BAR_CMD_REVERSE_SEARCH ||
            queryBar->cmd == QUERY_BAR_CMD_SEARCH_AND_REPLACE,
            "QueryBar is in incorrect state");
    
    char *str = toSearch;
    uint slen = toSearchLen;
    QueryBarCmdSearch *searchResult = &queryBar->searchCmd;
    QueryBarCommand id = queryBar->cmd;

    // Lookup for replace is always forward
    if(id == QUERY_BAR_CMD_SEARCH_AND_REPLACE){
        id = QUERY_BAR_CMD_SEARCH;
    }

    if(str == nullptr){
        QueryBar_GetWrittenContent(queryBar, &str, &slen);
    }

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
                if(buffer) start = buffer->taken-1;
                else start = 0;
            }else{
                done = 1;
            }
        }while(done == 0);
    }
    
    //printf("Invalid\n");
    //searchResult->valid = 0; // TODO: Why was this here?
    return 0;
}
