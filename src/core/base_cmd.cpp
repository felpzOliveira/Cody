#include <view.h>
#include <utilities.h>
#include <string.h>
#include <app.h>
#include <control_cmds.h>
#include <file_provider.h>
#include <parallel.h>
#include <bufferview.h>
#include <sstream>
#include <map>
#include <theme.h>
#include <dbgapp.h>
#include <graphics.h>
#include <storage.h>
#include <theme.h>

struct CmdInformation{
    std::string help;
    std::function<int(char *, uint, View *)> fn;
};

static std::map<std::string, std::string> aliasMap;
static std::string envDir;
static std::map<std::string, std::string> mathSymbolMap;
static std::map<std::string, CmdInformation> cmdMap;

struct QueriableSearchResult{
    std::vector<GlobalSearchResult> res;
    int count;
};

GlobalSearch results[MAX_THREADS];
QueriableSearchResult linearResults;


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
        mathSymbolMap["inf"] = "∞"; mathSymbolMap["approx"] = "≈";
        mathSymbolMap["or"] = "||";    mathSymbolMap["dash"] = "\\";
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
    linearResults.res.clear();
    linearResults.count = 0;
    for(int i = 0; i < MAX_THREADS; i++){
        results[i].results.clear();
        results[i].count = 0;
    }
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
        StorageDevice *device = FetchStorageDevice();
        std::string path = AppGetConfigFilePath();
        device->AppendTo(path.c_str(), entry);
    }

    return 0;
}

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

    uint s = BufferView_GetMaximumLineView(bView);
    int in = p.x + s;
    s = Clamp(in - 8, 0, BufferView_GetLineCount(bView)-1);
    BufferView_CursorToPosition(bView, s, p.y);

    BufferView_CursorToPosition(bView, p.x, p.y);
    BufferView_GhostCursorFollow(bView);
}

int GlobalSearchCommandCommit(QueryBar *queryBar, View *view){
    int active = View_SelectableListGetActiveIndex(view);
    if(active == -1){
        return 0;
    }

    active = View_SelectableListGetRealIndex(view, active);

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
    linearResults.res.clear();
    linearResults.count = 0;
    for(int i = 0; i < MAX_THREADS; i++){
        results[i].results.clear();
        results[i].count = 0;
    }
    return 1;
}

int SearchAllFilesCommandStart(View *view, std::string title){
    AssertA(view != nullptr, "Invalid view pointer");
    const char *header = title.c_str();
    uint hlen = strlen(header);
    LineBuffer *lineBuffer = AllocatorGetN(LineBuffer, 1);
    LineBuffer_InitBlank(lineBuffer);
    std::string root = AppGetRootDirectory();

    char *rootStr = (char *)root.c_str();
    uint rootLen = root.size();

    linearResults.count = 0;
    linearResults.res.clear();

    uint max_written_len = 60;
    int pathCompression = 1;
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
            uint pptr_size = strlen(pptr);
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
            uint filename_start = StringCompressPath(pptr, pptr_size, pathCompression);
            if(filename_start > 0){
                //len = snprintf(m, sizeof(m), "../");
            }

            if(start_loc == 0 || pickId == fid){
                len += snprintf(&m[len], sizeof(m)-len, "%s:%d: ", &pptr[filename_start],
                                res->results[i].line);
            }else{
                len += snprintf(&m[len], sizeof(m)-len, "%s:%d:... ", &pptr[filename_start],
                                res->results[i].line);
            }

            if(end_loc == f){
                len += snprintf(&m[len], sizeof(m)-len, "%s", &buffer->data[start_loc]);
            }else{
                len += snprintf(&m[len], sizeof(m)-len, "%s ...", &buffer->data[start_loc]);
            }

            //printf("Adding %s\n", m);

            buffer->data[end_loc] = ss;

            linearResults.res.push_back(res->results[i]);
            linearResults.count++;

            LineBuffer_InsertLine(lineBuffer, (char *)m, len);
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
    if(cmd[0] == '\\' || cmd[0] == '/'){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[1], size - 1);
        if(e < 0) return r;
        e += 1;

        std::string symname(&cmd[e]);
        InitializeMathSymbolList();

        if(mathSymbolMap.find(symname) != mathSymbolMap.end()){
            std::string value = mathSymbolMap[symname];
            AppPasteString(value.c_str(), value.size(), true);
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

int BaseCommand_Git(char *cmd, uint size, View *){
    int r = 1;
    uint len = 0;
    char *gitCmd = StringNextWord(cmd, size, &len);
    if(StringEqual(gitCmd, (char *)"diff", Min(len, 4))){
        if(len != 4) return r; // check for ill construted string
        BaseCommand_GitDiff(gitCmd, size-len);
    }else if(StringEqual(gitCmd, (char *)"status", Min(len, 6))){
        AppCommandGitStatus();
    }else if(StringEqual(gitCmd, (char *)"open", Min(len, 4))){
        int e = StringFirstNonEmpty(&gitCmd[4], size - 4);
        std::string val(&gitCmd[4+e]);
        if(val.size() > 0){
            char buf[PATH_MAX];
            char *res = realpath(val.c_str(), buf);
            (void) res;
            AppCommandGitOpenRoot(buf);
        }
    }
    return r;
}

int BaseCommand_SearchAllFiles(char *cmd, uint size, View *){
    int r = 1;
    std::string search(CMD_SEARCH_STR);
    int e = StringFirstNonEmpty(&cmd[search.size()], size - search.size());
    if(e < 0) return r;
    e += search.size();

    std::string searchStr(&cmd[e]);

    FileBufferList *bufferList = FileProvider_GetBufferList();
    uint maxlen = List_Size(bufferList->fList);

    std::unique_ptr<FileBuffer *[]> bufferptr(new FileBuffer*[maxlen]);
    FileBuffer **bufferArray = bufferptr.get();
    size = List_FetchAsArray(bufferList->fList, bufferArray, maxlen, 0);

    for(int i = 0; i < MAX_THREADS; i++){
        results[i].results.clear();
        results[i].count = 0;
    }

    const char *strPtr = searchStr.c_str();
    uint stringLen = searchStr.size();

    ParallelFor("String Seach", 0, size, [&](int i, int tid){
        LineBuffer *lineBuffer = bufferArray[i]->lineBuffer;
        if(tid > MAX_THREADS-1){
            printf("Ooops cannot query, too many threads\n");
            return;
        }

        GlobalSearch *threadResult = &results[tid];

        for(uint j = 0; j < lineBuffer->lineCount; j++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, j);
            if(buffer->taken > 0){
                int at = 0;
                uint start = 0;
                do{
                    at = StringSearch((char *)strPtr, &buffer->data[start],
                                      stringLen, buffer->taken - start);
                    if(at >= 0){
                        threadResult->results.push_back({
                            .lineBuffer = lineBuffer,
                            .line = j,
                            .col = at + start,
                        });
                        start += at + stringLen;
                        threadResult->count++;
                    }
                }while(at >= 0);
            }
        }
    });

    View *view = AppGetActiveView();
    uint count = 0;
    for(uint i = 0; i < MAX_THREADS; i++){
        count += results[i].count;
    }

    std::stringstream ss;
    ss << "Search Files ( " << count << " )";
    if(SearchAllFilesCommandStart(view, ss.str().c_str()) >= 0){
        AppSetBindingsForState(View_SelectableList);
        r = 2;
    }

    return r;
}

int BaseCommand_SearchFunctions(char *cmd, uint size, View *){
    int r = 1;
    char *strPtr = nullptr;
    uint stringLen = 0;

    std::vector<std::string> splits;
    StringSplit(std::string(cmd), splits);
    FileBufferList *bufferList = FileProvider_GetBufferList();
    uint maxlen = List_Size(bufferList->fList);

    std::unique_ptr<FileBuffer *[]> bufferptr(new FileBuffer*[maxlen]);
    FileBuffer **bufferArray = bufferptr.get();
    size = List_FetchAsArray(bufferList->fList, bufferArray, maxlen, 0);

    for(int i = 0; i < MAX_THREADS; i++){
        results[i].results.clear();
        results[i].count = 0;
    }

    if(splits.size() > 1){
        strPtr = (char *)splits[1].c_str();
        stringLen = splits[1].size();
    }

    ParallelFor("Functions Search", 0, size, [&](int i, int tid){
        LineBuffer *lineBuffer = bufferArray[i]->lineBuffer;
        if(tid > MAX_THREADS-1){
            printf("Ooops cannot query, too many threads\n");
            return;
        }

        GlobalSearch *threadResult = &results[tid];
        for(uint j = 0; j < lineBuffer->lineCount; j++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, j);
            for(uint k = 0; k < buffer->tokenCount; k++){
                Token *token = &buffer->tokens[k];
                int found = 0;
                if(token->identifier == TOKEN_ID_FUNCTION_DECLARATION){
                    found = 1;
                    if(strPtr){
                        char *s = &buffer->data[token->position];
                        found = StringEqual(strPtr, s, Min(token->size, stringLen));
                    }
                }

                if(found){
                    threadResult->results.push_back({
                        .lineBuffer = lineBuffer,
                        .line = j,
                        .col = (uint)(token->position < 0 ? 0 : token->position),
                    });
                    threadResult->count++;
                }
            }
        }
    });

    View *view = AppGetActiveView();
    uint count = 0;
    for(uint i = 0; i < MAX_THREADS; i++){
        count += results[i].count;
    }

    std::stringstream ss;
    ss << "Functions ( " << count << " )";
    if(SearchAllFilesCommandStart(view, ss.str().c_str()) >= 0){
        AppSetBindingsForState(View_SelectableList);
        r = 2;
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
        //printf("Alias [%s] = [%s]\n", targetStr.c_str(), valueStr.c_str());
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

int BaseCommand_SetDimm(char *cmd, uint size, View *){
    int r = 1;
    uint len = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringEqual(arg, (char *)"on", Min(len, 2))){
        CurrentThemeSetDimm(1);
    }else if(StringEqual(arg, (char *)"off", Min(len, 3))){
        CurrentThemeSetDimm(0);
    }
    return r;
}

int BaseCommand_CursorSetFormat(char *cmd, uint size, View *){
    int r = 1;
    uint len = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringEqual(arg, (char *)"quad", Min(len, 4))){
        AppSetCursorStyle(CURSOR_QUAD);
    }else if(StringEqual(arg, (char *)"dash", Min(len, 4))){
        AppSetCursorStyle(CURSOR_DASH);
    }else if(StringEqual(arg, (char *)"rect", Min(len, 4))){
        AppSetCursorStyle(CURSOR_RECT);
    }
    return r;
}

int BaseCommand_ChangeFontSize(char *cmd, uint size, View *){
    uint len = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringIsDigits(arg, len)){
        uint size = StringToUnsigned(arg, len);
        AppSetFontSize(size);
    }
    return 1;
}

int BaseCommand_ChangeBrightness(char *cmd, uint size, View *){
    uint len = 0;
    Float fvalue = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringToFloat(arg, arg+len, &fvalue)){
        CurrentThemeSetBrightness(fvalue);
    }

    return 1;
}

int BaseCommand_ChangeContrast(char *cmd, uint size, View *){
    uint len = 0;
    Float fvalue = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringToFloat(arg, arg+len, &fvalue)){
        CurrentThemeSetContrast(fvalue);
    }

    return 1;
}

int BaseCommand_ChangeSaturation(char *cmd, uint size, View *){
    uint len = 0;
    Float fvalue = 0;
    char *arg = StringNextWord(cmd, size, &len);
    if(StringToFloat(arg, arg+len, &fvalue)){
        CurrentThemeSetSaturation(fvalue);
    }

    return 1;
}

int BaseCommand_PathCompression(char *cmd, uint size, View *){
    int r = 1;
    uint len = 0;
    bool negative = false;
    char *arg = StringNextWord(cmd, size, &len);
    char *ptr = arg;
    uint val = 0;
    if(*arg == '-' && len > 1){
        ptr = &arg[1];
        negative = true;
        len -= 1;
    }

    if(StringIsDigits(ptr, len)){
        val = StringToUnsigned(ptr, len);
    }else{
        return r;
    }

    int pick = negative ? -val : val;
    AppSetPathCompression(pick);
    return r;
}

int BaseCommand_DbgStart(char *cmd, uint size, View *view){
    int r = 1;
    char pmax[PATH_MAX];
    memset(pmax, 0, PATH_MAX);
    std::vector<std::string> splits;
    StringSplit(std::string(cmd), splits);
    if(splits.size() < 3){
        printf("Invalid arguments\n");
        return r;
    }

    std::string arg0 = splits[2];
    std::string arg1;
    for(uint i = 3; i < splits.size(); i++){
        arg1 += splits[i];
        if(i < splits.size()-1) arg1 += " ";
    }
    printf("Running %s %s\n", arg0.c_str(), arg1.c_str());
    char *p = realpath(arg0.c_str(), pmax);
    if(!p){
        printf("Could not translate path [ %s ]\n", arg0.c_str());
        return r;
    }

    DbgApp_StartDebugger(pmax, arg1.c_str());

    return r;
}

int BaseCommand_DbgBreak(char *cmd, uint size, View *view){
    int r = 1;
    std::vector<std::string> splits;
    StringSplit(std::string(cmd), splits);
    if(splits.size() != 4){
        printf("Invalid arguments\n");
        return r;
    }

    const char *p0 = splits[2].c_str();
    const char *p1 = splits[3].c_str();
    uint val = StringToUnsigned((char *)p1, splits[3].size());
    DbgApp_Break((char *)p0, val);

    return r;
}

int BaseCommand_DbgEvaluate(char *cmd, uint size, View *view){
    int r = 1;
    std::string arg;
    std::vector<std::string> splits;
    StringSplit(std::string(cmd), splits);
    for(uint i = 2; i < splits.size(); i++){
        arg += std::string(splits[i]);
        if(i < splits.size() - 1) arg += " ";
    }

    DbgApp_Eval((char *)arg.c_str(), 0);
    return r;
}

int BaseCommand_DbgFinish(char *cmd, uint size, View *view){
    DbgApp_Finish();
    return 1;
}

int BaseCommand_DbgExit(char *cmd, uint size, View *view){
    DbgApp_Interrupt();
    DbgApp_Exit();
    return 1;
}

int BaseCommand_DbgRun(char *cmd, uint size, View *view){
    DbgApp_Run();
    return 1;
}

int BaseCommand_KillSpaces(char *cmd, uint size, View *view){
    int r = 1;
    BufferView *bView = View_GetBufferView(view);
    if(bView){
        LineBuffer *lineBuffer = BufferView_GetLineBuffer(bView);
        if(!lineBuffer) return r;

        uint lineCount = BufferView_GetLineCount(bView);
        bool any = false;

        for(uint i = 0; i < lineCount; i++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
            if(!buffer) continue;
            if(Buffer_IsBlank(buffer)){
                Buffer_SoftClear(buffer);
                any = true;
            }else if(buffer->tokenCount > 1){
                Token *lastToken = &buffer->tokens[buffer->tokenCount-1];
                if(lastToken->identifier == TOKEN_ID_SPACE){
                    Buffer_RemoveLastToken(buffer);
                    any = true;
                }
            }
        }

        if(any){
            BufferView_Dirty(bView);
        }
    }
    return r;
}

/* These are most duplicated from app.cpp so we get a list of strings attached to a cmd */
int BaseCommand_Vsplit(char *, uint, View *){
    AppCommandSplitVertical();
    return 1;
}

int BaseCommand_Hsplit(char *, uint, View *){
    AppCommandSplitHorizontal();
    return 1;
}

int BaseCommand_ExpandRestore(char *, uint, View *){
    ControlCommands_SwapExpand();
    return 1;
}

int BaseCommand_KillView(char *, uint, View *){
    AppCommandKillView();
    return 1;
}

int BaseCommand_KillBuffer(char *, uint, View *){
    AppCommandKillBuffer();
    return 1;
}

int BaseCommand_CursorSegmentToogle(char *, uint, View *){
    Graphics_ToogleCursorSegment();
    return 1;
}

int BaseCommand_ToogleWrongIdent(char *, uint, View *){
    int val = AppGetDisplayWrongIdent() > 0 ? 1 : 0;
    AppSetDisplayWrongIdent(1 - val);
    return 1;
}

int BaseCommand_IndentFile(char *, uint, View *){
    AppCommandIndentCurrent();
    return 1;
}

int BaseCommand_IndentRegion(char *, uint, View *){
    AppCommandIndent();
    return 1;
}

int BaseCommand_CursorBlink(char *, uint, View *){
    Graphics_ToogleCursorBlinking();
    return 1;
}

void BaseCommand_InitializeCommandMap(){
    cmdMap[CMD_DIMM_STR] = {CMD_DIMM_HELP, BaseCommand_SetDimm};
    cmdMap[CMD_KILLSPACES_STR] = {CMD_KILLSPACES_HELP, BaseCommand_KillSpaces};
    cmdMap[CMD_SEARCH_STR] = {CMD_SEARCH_HELP, BaseCommand_SearchAllFiles};
    cmdMap[CMD_FUNCTIONS_STR] = {CMD_FUNCTIONS_HELP, BaseCommand_SearchFunctions};
    cmdMap[CMD_GIT_STR] = {CMD_GIT_HELP, BaseCommand_Git};
    cmdMap[CMD_HSPLIT_STR] = {CMD_HSPLIT_HELP, BaseCommand_Hsplit};
    cmdMap[CMD_VSPLIT_STR] = {CMD_VSPLIT_HELP, BaseCommand_Vsplit};
    cmdMap[CMD_EXPAND_STR] = {CMD_EXPAND_HELP, BaseCommand_ExpandRestore};
    cmdMap[CMD_KILLVIEW_STR] = {CMD_KILLVIEW_HELP, BaseCommand_KillView};
    cmdMap[CMD_KILLBUFFER_STR] = {CMD_KILLBUFFER_HELP, BaseCommand_KillBuffer};
    cmdMap[CMD_DBG_START_STR] = {CMD_DBG_START_HELP, BaseCommand_DbgStart};
    cmdMap[CMD_DBG_BREAK_STR] = {CMD_DBG_BREAK_HELP, BaseCommand_DbgBreak};
    cmdMap[CMD_DBG_EXIT_STR] = {CMD_DBG_EXIT_HELP, BaseCommand_DbgExit};
    cmdMap[CMD_DBG_RUN_STR] = {CMD_DBG_RUN_HELP, BaseCommand_DbgRun};
    cmdMap[CMD_DBG_FINISH_STR] = {CMD_DBG_FINISH_HELP, BaseCommand_DbgFinish};
    cmdMap[CMD_DBG_EVALUATE_STR] = {CMD_DBG_EVALUATE_HELP, BaseCommand_DbgEvaluate};
    cmdMap[CMD_CURSORSEG_STR] = {CMD_CURSORSEG_HELP, BaseCommand_CursorSegmentToogle};
    cmdMap[CMD_CURSORSET_STR] = {CMD_CURSORSET_HELP, BaseCommand_CursorSetFormat};
    cmdMap[CMD_PATH_COMPRESSION_STR] = {CMD_PATH_COMPRESSION_HELP, BaseCommand_PathCompression};
    cmdMap[CMD_WRONGIDENT_DISPLAY_STR] = {CMD_WRONGIDENT_DISPLAY_HELP, BaseCommand_ToogleWrongIdent};
    cmdMap[CMD_CHANGE_FONTSIZE_STR] = {CMD_CHANGE_FONTSIZE_HELP, BaseCommand_ChangeFontSize};
    cmdMap[CMD_CHANGE_CONTRAST_STR] = {CMD_CHANGE_CONTRAST_HELP, BaseCommand_ChangeContrast};
    cmdMap[CMD_CHANGE_BRIGHTNESS_STR] = {CMD_CHANGE_BRIGHTNESS_HELP, BaseCommand_ChangeBrightness};
    cmdMap[CMD_CHANGE_SATURATION_STR] = {CMD_CHANGE_SATURATION_HELP, BaseCommand_ChangeSaturation};
    cmdMap[CMD_INDENT_FILE_STR] = {CMD_INDENT_FILE_HELP, BaseCommand_IndentFile};
    cmdMap[CMD_INDENT_REGION_STR] = {CMD_INDENT_REGION_HELP, BaseCommand_IndentRegion};
    cmdMap[CMD_CURSOR_BLINK_STR] = {CMD_CURSOR_BLINK_HELP, BaseCommand_CursorBlink};
}

int BaseCommand_Interpret(char *cmd, uint size, View *view){
    // TODO: map of commands? maybe set some function pointers to this
    // TODO: create a standard for this, a json or at least something like bubbles
    int rv = -1;
    if(BaseCommand_Interpret_AliasInsert(cmd, size, view)){
        QueryBar_EnableInputToHistory(View_GetQueryBar(view));
        return 0;
    }else if(BaseCommand_SetExecPath(cmd, size)){
        return 0;
    }else if(BaseCommand_InsertMappedSymbol(cmd, size)){
        return 0;
    }else{
        for(auto it = cmdMap.begin(); it != cmdMap.end(); it++){
            std::string val = it->first;
            char *ptr = (char *)val.c_str();
            if(StringStartsWith(cmd, size, ptr, Min(size, val.size()))){
                CmdInformation cmdInfo = it->second;
                /* commands that are explicitly typed should be added to history */
                QueryBar *targetBar = View_GetQueryBar(view);
                QueryBar_EnableInputToHistory(targetBar);
                rv = cmdInfo.fn(cmd, size, view);
                return rv;
            }
        }
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

    BufferView_SwapBuffer(bView, lockedBuffer->lineBuffer, CodeView);
    // because the parallel thread will reset the linebuffer we need
    // to make sure the cursor is located in a valid range for rendering
    // otherwise in case the build buffer does updates too fast it can
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
    StorageDevice *storage = FetchStorageDevice();
    std::string refPath(basePath);
    if(refPath[refPath.size()-1] != '/'){
        refPath += "/";
    }

    if(storage->ListFiles((char *)refPath.c_str(), entries, n, size) < 0){
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
        LineBuffer_InsertLine(lb, e->path, e->pLen);
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
        StorageDevice *storage = FetchStorageDevice();
        if(len < opener->pathLen){ // its a character remove update
            // check if we can erase the folder path
            if(opener->basePath[opener->pathLen-1] == '/'){ // change dir
                char tmp = 0;
                uint n = GetSimplifiedPathName(opener->basePath, opener->pathLen-1);

                tmp = opener->basePath[n];
                opener->basePath[n] = 0;
                if(storage->Chdir(opener->basePath) < 0){
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
                    IGNORE(storage->Chdir(opener->basePath));
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

            if(storage->Chdir(content) < 0){
                printf("Failed to change to %s directory\n", content);
                return -1;
            }

            if(ListFileEntriesAndCheckLoaded(content, &opener->entries,
                                             &opener->entryCount, &opener->entrySize) < 0)
            {
                printf("Failed to list files from %s\n", content);
                IGNORE(storage->Chdir(opener->basePath));
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
    StorageDevice *storage = FetchStorageDevice();
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
        if(storage->Chdir(opener->basePath) < 0){
            printf("Failed to change to %s directory\n", opener->basePath);
            goto end;
        }

        if(ListFileEntriesAndCheckLoaded(opener->basePath, &opener->entries,
                                         &opener->entryCount, &opener->entrySize) < 0)
        {
            printf("Failed to list files from %s\n", opener->basePath);
            IGNORE(storage->Chdir(opener->basePath));
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
        LineBuffer_InsertLine(lineBuffer, e->path, e->pLen);
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
                LineBuffer_InsertLine(lineBuffer, &ptr[n], len - n);
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
        LineBuffer_InsertLine(lineBuffer, (char *)dsc.name, slen);
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    filter.allowFreeType = 0;
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           SwitchThemeCommandCommit, &filter);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Search text command routines.
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
