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
#include <resources.h>
#include <rng.h>
#include <cryptoutil.h>
#include <unordered_map>
#include <set>
#include <audio.h>

int Mkdir(const char *path);
char* __realpath(const char* path, char* resolved_path);

static std::map<std::string, std::string> aliasMap;
static std::string envDir;
static std::map<std::string, std::string> lookupSymbol;
static std::map<std::string, CmdInformation> cmdMap;

struct QueriableSearchResult{
    std::vector<GlobalSearchResult> res;
    int count;
};

GlobalSearch results[MAX_THREADS];
QueriableSearchResult linearResults;


static void InitializeMathSymbolList(){
    static bool is_lookup_symbols_inited = false;
    if(!is_lookup_symbols_inited){
        lookupSymbol["pi"] = "π";    lookupSymbol["Pi"] = "Π";
        lookupSymbol["theta"] = "θ"; lookupSymbol["Theta"] = "Θ";
        lookupSymbol["mu"] = "μ";    lookupSymbol["Mu"] = "M";
        lookupSymbol["zeta"] = "ζ";  lookupSymbol["Zeta"] = "Z";
        lookupSymbol["eta"] = "η";   lookupSymbol["Eta"] = "H";
        lookupSymbol["iota"] = "ι";  lookupSymbol["Iota"] = "I";
        lookupSymbol["kappa"] = "κ"; lookupSymbol["Kappa"] = "K";
        lookupSymbol["lambda"] = "λ";lookupSymbol["Lambda"] = "Λ";
        lookupSymbol["nu"] = "ν";    lookupSymbol["Nu"] = "N";
        lookupSymbol["xi"] = "ξ";    lookupSymbol["Xi"] = "Ξ";
        lookupSymbol["rho"] = "ρ";   lookupSymbol["Rho"] = "P";
        lookupSymbol["sigma"] = "σ"; lookupSymbol["Sigma"] = "Σ";
        lookupSymbol["tau"] = "τ";   lookupSymbol["Tau"] = "T";
        lookupSymbol["phi"] = "φ";   lookupSymbol["Phi"] = "Φ";
        lookupSymbol["chi"] = "χ";   lookupSymbol["Chi"] = "X";
        lookupSymbol["psi"] = "ψ";   lookupSymbol["Psi"] = "Ψ";
        lookupSymbol["omega"] = "ω"; lookupSymbol["Omega"] = "Ω";
        lookupSymbol["beta"] = "β";  lookupSymbol["Beta"] = "B";
        lookupSymbol["alpha"] = "α"; lookupSymbol["Alpha"] = "A";
        lookupSymbol["gamma"] = "γ"; lookupSymbol["Gamma"] = "Γ";
        lookupSymbol["delta"] = "δ"; lookupSymbol["Delta"] = "Δ";
        lookupSymbol["epsilon"] = "ε"; lookupSymbol["Epsilon"] = "E";
        lookupSymbol["partial"] = "∂"; lookupSymbol["int"] = "∫";
        lookupSymbol["inf"] = "∞";     lookupSymbol["approx"] = "≈";
        lookupSymbol["cdot"] = "·";    lookupSymbol["or"] = "||";
        lookupSymbol["dash"] = "\\"; lookupSymbol["sqrt"] = "√";
        lookupSymbol["rarrow"] = "→"; lookupSymbol["larrow"] = "←";
        lookupSymbol["e2"] = "²"; lookupSymbol["e3"] = "³";
        lookupSymbol["e4"] = "⁴"; lookupSymbol["e5"] = "⁵";
        lookupSymbol["e6"] = "⁶"; lookupSymbol["e7"] = "⁷";
        lookupSymbol["ctimes"] = "×"; lookupSymbol["e-"] = "⁻";
        lookupSymbol["e+"] = "⁺";
        is_lookup_symbols_inited = true;
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

    View_SetAllowPathCompression(view, true);
    return 1;
}

std::map<std::string, CmdInformation> *BaseCommand_GetCmdMap(){
    return &cmdMap;
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
        rv = Mkdir(dir.c_str());
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
    GlobalSearchResult *result = nullptr;
    LineBuffer *targetLineBuffer = nullptr;
    int active = View_SelectableListGetActiveIndex(view);
    if(active == -1){
        return 0;
    }

    active = View_SelectableListGetRealIndex(view, active);
    if(active >= linearResults.count){
        goto __ret;
    }

    result = &linearResults.res[active];
    targetLineBuffer = result->lineBuffer;

    BaseCommand_JumpViewToBuffer(view, targetLineBuffer,
                                 vec2i(result->line, result->col));
    View_SetAllowPathCompression(view, true);
__ret:
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

    View_SetAllowPathCompression(view, false);

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
                while(at < l && (p[at] == '/' || p[at] == '\\')) at ++;
            }

            LineBuffer *lBuffer = res->results[i].lineBuffer;
            EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(lBuffer);
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

            uint sid = Buffer_Utf8RawPositionToPosition(buffer, start_loc, encoder);
            uint tid = Buffer_GetTokenAt(buffer, sid, encoder);
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

            buffer->data[end_loc] = ss;

            linearResults.res.push_back(res->results[i]);
            linearResults.count++;

            LineBuffer_InsertLine(lineBuffer, (char *)m, len);
        }
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    filter.toHistory = false;
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

        if(lookupSymbol.find(symname) != lookupSymbol.end()){
            std::string value = lookupSymbol[symname];
            AppPasteString(value.c_str(), value.size(), true);
        }
    }

    return r;
}

int BaseCommand_GlobalEncodingSwap(char *cmd, uint size, View *view){
    int r = 1;
    std::string encoding(CMD_GLOBAL_ENCODING_STR);

    int e = StringFirstNonEmpty(&cmd[encoding.size()], size - encoding.size());
    if(e < 0) return r;

    e += encoding.size();
    std::string target(&cmd[e]);

    SetGlobalDefaultEncoding(target);
    return r;
}

int BaseCommand_EncodingSwap(char *cmd, uint size, View *view){
    int r = 1;
    std::string encoding(CMD_ENCODING_STR);
    int e = StringFirstNonEmpty(&cmd[encoding.size()], size - encoding.size());
    if(e < 0) return r;
    e += encoding.size();

    std::string target(&cmd[e]);
    BufferView *bView = View_GetBufferView(view);

    if(!bView) return r;

    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bView);

    if(!lineBuffer) return r;

    EncoderDecoderType type = EncoderDecoder_GetType(target);
    if(type == ENCODER_DECODER_NONE)
        return r;

    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(lineBuffer);
    EncoderDecoder_InitFor(encoder, type);

    LineBuffer_UpdateEncoding(lineBuffer);
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
    cmd[size] = 0;
    if(StringStartsWith(cmd, size, (char *)alias.c_str(), alias.size())){
        r = 1;
        int e = StringFirstNonEmpty(&cmd[alias.size()], size - alias.size());
        if(e < 0) return r;
        e += alias.size();

        char *target = &cmd[e];

        SkipWhiteSpaces(&target);
        if(*target == 0)
            return r;

        char *ref = target;
        SkipUntill(&ref, '=');
        if(*ref == 0)
            return r;

        *ref = 0;
        // skip backwards spaces
        char *aux = ref-1;
        while(*aux == ' '){
            *aux = 0;
            aux -= 1;
        }

        std::string targetStr(target);
        ref += 1;
        SkipWhiteSpaces(&ref);

        std::string valueStr(ref);

        aliasMap[targetStr] = valueStr;
        //printf("Alias [%s] = [%s]\n", targetStr.c_str(), valueStr.c_str());
    }

    return r;
}

std::string BaseCommand_GetBasePath(){
    if(envDir.size() > 0) return std::string(SEPARATOR_STRING) + envDir;
    return SEPARATOR_STRING "build";
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

int SwapFontInternal(std::string fontname){
    // NOTE: hard-coded fonts
    if(fontname == "default"){
        Graphics_SetFont((char *)FONT_liberation_mono_ttf, FONT_liberation_mono_ttf_len);
        return 1;
    }else if(fontname == "consolas"){
        Graphics_SetFont((char *)FONT_consolas_ttf, FONT_consolas_ttf_len);
        return 1;
    }else if(fontname == "commit"){
        Graphics_SetFont((char *)FONT_commit_mono_ttf, FONT_commit_mono_ttf_len);
        return 1;
    }else if(fontname == "ubuntu"){
        Graphics_SetFont((char *)FONT_ubuntu_mono_ttf, FONT_ubuntu_mono_ttf_len);
        return 1;
    }else if(fontname == "dejavu"){
        Graphics_SetFont((char *)FONT_dejavu_sans_ttf, FONT_dejavu_sans_ttf_len);
        return 1;
    }else if(fontname == "jet"){
        Graphics_SetFont((char *)FONT_jet_mono_ttf, FONT_jet_mono_ttf_len);
        return 1;
    }

    return 0;
}

int BaseCommand_ChangeFont(char *cmd, uint size, View *){
    int r = 1;
    uint len = 0;
    char *arg = StringNextWord(cmd, size, &len);

    std::string path(arg, len);

    if(SwapFontInternal(path))
        return r;

    if(!FileExists((char *)path.c_str()))
        return r;

    len = 0;
    // NOTE: The ttf pointer needs to be alive as it is stored internally
    //       by the font rendering code.
    char *ttf = GetFileContents((char *)path.c_str(), &len);
    if(!ttf)
        return r;

    Graphics_SetFont(ttf, len);
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
    char *p = __realpath(arg0.c_str(), pmax);
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
        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(lineBuffer);
        bool any = false;

        for(uint i = 0; i < lineCount; i++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
            if(!buffer) continue;
            if(Buffer_IsBlank(buffer)){
                Buffer_SoftClear(buffer, encoder);
                any = true;
            }else if(buffer->tokenCount > 1){
                Token *lastToken = &buffer->tokens[buffer->tokenCount-1];
                if(lastToken->identifier == TOKEN_ID_SPACE){
                    Buffer_RemoveLastToken(buffer, encoder);
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

int BaseCommandMusicMng_Add(char *cmd, uint size, View *view){
    int r = 0;
    const char *strCmd = CMD_MUSIC_ADD;
    const uint len = strlen(strCmd);
    if(StringStartsWith(cmd, size, (char *)strCmd, len)){
        int e = StringFirstNonEmpty(&cmd[len], size-len);
        e += len;

        if(FileExists(&cmd[e])){
            if(size-len+1 < AUDIO_MESSAGE_MAX_SIZE){
                AudioMessage message;
                InitializeAudioSystem();

                message.code = AUDIO_CODE_PLAY;
                int n = snprintf((char *)message.argument,
                                 AUDIO_MESSAGE_MAX_SIZE, "%s", &cmd[e]);
                message.argument[n] = 0;
                AudioRequest(message);

                r = 1;
            }else{
                printf("[BUG] File path larger than audio message!\n");
            }
        }
    }
    return r;
}

int BaseCommandMusicMng_Pause(char *, uint, View *){
    if(AudioIsRunning()){
        AudioMessage message;
        message.code = AUDIO_CODE_PAUSE;
        AudioRequest(message);
    }
    return 1;
}

int BaseCommandMusicMng_Resume(char *, uint, View *){
    if(AudioIsRunning()){
        AudioMessage message;
        message.code = AUDIO_CODE_RESUME;
        AudioRequest(message);
    }
    return 1;
}

int BaseCommandMusicMng_Stop(char *, uint, View *){
    if(AudioIsRunning()){
        AudioMessage message;
        message.code = AUDIO_CODE_FINISH;
        AudioRequest(message);
    }
    return 1;
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

int BaseCommandSwapTabs(char *, uint, View *){
    AppSwapUseTabs();
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

int BaseCommand_HistoryClear(char *, uint, View *){
    AppClearHistory();
    return 1;
}

static
std::string GetPwdDir(StorageDevice *device){
    char dir[PATH_MAX];
    uint len = PATH_MAX;
    memset(dir, 0, PATH_MAX);
    if(device)
        device->GetWorkingDirectory(dir, len);
    else
        GetCurrentWorkingDirectory(dir, len);
    return std::string(dir);
}

int BaseCommandPutFile(char *cmd, uint size, View *view){
    uint fsize = 0;
    StorageDevice *device = FetchStorageDevice();
    if(device->IsLocallyStored())
        return 1;

    std::string put(CMD_PUT_STR);
    int e = StringFirstNonEmpty(&cmd[put.size()], size - put.size());
    if(e < 0)
        return 1;

    e += put.size();

    char *content = GetFileContents(&cmd[e], &fsize);
    if(content == nullptr || fsize < 1)
        return 1;

    uint where = GetSimplifiedPathName(&cmd[e], size - e);

    std::string path(GetPwdDir(device));
    std::string fileName(&cmd[e + where], size - e - where);

    path += std::string(SEPARATOR_STRING) + fileName;
    std::cout << "[Debug] Path= " << path << std::endl;

    SaveToStorageImpl((char *)path.c_str(), path.size(),
                      (uint8_t *)content, fsize);

    AllocatorFree(content);
    return 1;
}

int BaseCommandJumpTo(char *cmd, uint size, View *view){
    std::string jump_to(CMD_JUMP_TO_STR);
    int e = StringFirstNonEmpty(&cmd[jump_to.size()], size - jump_to.size());
    if(e < 0)
        return 1;

    e += jump_to.size();
    char *target = &cmd[e];

    SkipWhiteSpaces(&target);
    if(*target == 0)
        return 1;

    int value = atoi(target);
    if(value < 0)
        return 1;

    BufferView *bView = View_GetBufferView(view);
    if(BufferView_GetViewType(bView) == ImageView){
        PdfViewState *pdfView = bView->pdfView;
        if(!pdfView)
            return 1;

        PdfView_OpenPage(pdfView, value);
    }else{
        uint maxn = BufferView_GetLineCount(bView);
        value = value > 0 ? value-1 : 0;
        value = Clamp(value, (uint)0, maxn-1);
        Buffer *buffer = BufferView_GetBufferAt(bView, value);
        uint p = Buffer_FindFirstNonEmptyLocation(buffer);
        BufferView_CursorToPosition(bView, value, p);
    }

    return 1;
}

int BaseCommandGetFile(char *cmd, uint size, View *view){
    uint fsize = 0;
    StorageDevice *device = FetchStorageDevice();
    if(device->IsLocallyStored())
        return 1;

    std::string get(CMD_GET_STR);
    int e = StringFirstNonEmpty(&cmd[get.size()], size - get.size());
    if(e < 0)
        return 1;

    e += get.size();
    uint where = GetSimplifiedPathName(&cmd[e], size - e);

    std::string path(GetPwdDir(nullptr));
    std::string fileName(&cmd[e + where], size - e - where);
    path += std::string(SEPARATOR_STRING) + fileName;

    char *content = device->GetContentsOf(&cmd[e], &fsize);

    if(content == nullptr || fsize < 1)
        return 1;

    std::cout << "[Debug] Path= " << path << std::endl;
    WriteFileContents(path.c_str(), content, fsize);
    return 1;
}

int BaseCommandSetEncrypted(char *cmd, uint size, View *view){
    uint8_t key[32];
    uint8_t salt[CRYPTO_SALT_LEN];
    std::string enc(CMD_SET_ENCRYPT_STR);
    BufferView *bView = View_GetBufferView(view);
    if(bView->lineBuffer == nullptr)
        return 1;

    int e = StringFirstNonEmpty(&cmd[enc.size()], size - enc.size());
    if(e < 0)
        return 1;

    e += enc.size();
    std::string target(&cmd[e]);

    QueryBar *qbar = View_GetQueryBar(view);
    qbar->filter.toHistory = false;

    Crypto_SecureRNG(salt, CRYPTO_SALT_LEN);
    CryptoUtil_PasswordHash((char *)target.c_str(), target.size(), key, salt);

    LineBuffer_SetEncrypted(bView->lineBuffer, key, salt);
    return 1;
}

void BaseCommand_InitializeCommandMap(){
    cmdMap[CMD_DIMM_STR] = {CMD_DIMM_HELP, BaseCommand_SetDimm};
    cmdMap[CMD_KILLSPACES_STR] = {CMD_KILLSPACES_HELP, BaseCommand_KillSpaces};
    cmdMap[CMD_SEARCH_STR] = {CMD_SEARCH_HELP, BaseCommand_SearchAllFiles};
    cmdMap[CMD_ENCODING_STR] = {CMD_ENCODING_HELP, BaseCommand_EncodingSwap};
    cmdMap[CMD_GLOBAL_ENCODING_STR] = {CMD_GLOBAL_ENCODING_HELP, BaseCommand_GlobalEncodingSwap};
    cmdMap[CMD_FUNCTIONS_STR] = {CMD_FUNCTIONS_HELP, BaseCommand_SearchFunctions};
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
    cmdMap[CMD_CHANGE_FONT_STR] = {CMD_CHANGE_FONT_HELP, BaseCommand_ChangeFont};
    cmdMap[CMD_CHANGE_CONTRAST_STR] = {CMD_CHANGE_CONTRAST_HELP, BaseCommand_ChangeContrast};
    cmdMap[CMD_CHANGE_BRIGHTNESS_STR] = {CMD_CHANGE_BRIGHTNESS_HELP, BaseCommand_ChangeBrightness};
    cmdMap[CMD_CHANGE_SATURATION_STR] = {CMD_CHANGE_SATURATION_HELP, BaseCommand_ChangeSaturation};
    cmdMap[CMD_INDENT_FILE_STR] = {CMD_INDENT_FILE_HELP, BaseCommand_IndentFile};
    cmdMap[CMD_INDENT_REGION_STR] = {CMD_INDENT_REGION_HELP, BaseCommand_IndentRegion};
    cmdMap[CMD_CURSOR_BLINK_STR] = {CMD_CURSOR_BLINK_HELP, BaseCommand_CursorBlink};
    cmdMap[CMD_HISTORY_CLEAR_STR] = {CMD_HISTORY_CLEAR_HELP, BaseCommand_HistoryClear};
    cmdMap[CMD_SET_ENCRYPT_STR] = {CMD_SET_ENCRYPT_HELP, BaseCommandSetEncrypted};
    cmdMap[CMD_PUT_STR] = {CMD_PUT_HELP, BaseCommandPutFile};
    cmdMap[CMD_GET_STR] = {CMD_GET_HELP, BaseCommandGetFile};
    cmdMap[CMD_JUMP_TO_STR] = {CMD_JUMP_TO_HELP, BaseCommandJumpTo};
    cmdMap[CMD_SWAP_TABS] = {CMD_SWAP_TABS_HELP, BaseCommandSwapTabs};
    cmdMap[CMD_MUSIC_ADD] = {CMD_MUSIC_ADD_HELP, BaseCommandMusicMng_Add};
    cmdMap[CMD_MUSIC_PAUSE] = {CMD_MUSIC_PAUSE_HELP, BaseCommandMusicMng_Pause};
    cmdMap[CMD_MUSIC_RESUME] = {CMD_MUSIC_RESUME_HELP, BaseCommandMusicMng_Resume};
    cmdMap[CMD_MUSIC_STOP] = {CMD_MUSIC_STOP_HELP, BaseCommandMusicMng_Stop};
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
        uint at = 0;
        for(uint i = 0; i < size; i++){
            if(cmd[i] == ' ')
                break;
            at++;
        }

        if(StringStartsWith(cmd, size, (char *)"help", 4)){
            AppSetDelayedCall(AppCommandListHelp);
            return 1;
        }

        for(auto it = cmdMap.begin(); it != cmdMap.end(); it++){
            std::string val = it->first;
            char *ptr = (char *)val.c_str();
            if(at > val.size())
                continue;

            if(StringStartsWith(cmd, size, ptr, at)){
                CmdInformation cmdInfo = it->second;
                /* commands that are explicitly typed should be added to history */
                QueryBar *targetBar = View_GetQueryBar(view);
                QueryBar_EnableInputToHistory(targetBar);
                rv = cmdInfo.fn(cmd, size, view);
                return rv;
            }
        }
    }

    bool swapViews = true;
    std::string strCmd = BaseCommand_Interpret_Alias(cmd, size);
    if(strCmd.size() > 4 &&
       StringStartsWith((char *)strCmd.c_str(), strCmd.size(), (char *)"[nv]", 4))
    {
        strCmd = strCmd.substr(4);
        swapViews = false;
    }

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
            BufferView *tmpBView = View_GetBufferView(vnode->view);
            if(View_GetState(vnode->view) != View_ImageDisplay &&
               BufferView_GetViewType(tmpBView) != ImageView)
            {
                bView = tmpBView; // NOTE: This will overwrite the current view
            }
        }
    }

    if(swapViews && BufferView_GetViewType(bView) != ImageView){
        BufferView_SwapBuffer(bView, lockedBuffer->lineBuffer, CodeView);
        // because the parallel thread will reset the linebuffer we need
        // to make sure the cursor is located in a valid range for rendering
        // otherwise in case the build buffer does updates too fast it can
        // generate a SIGSEGV. This needs to run after the swap as we need
        // to make sure the position cache map is updated with whatever
        // is being rendered at the moment.
        BufferView_CursorToPosition(bView, 0, 0);
        BufferView_GhostCursorFollow(bView);
    }

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
    if(refPath[refPath.size()-1] != '/' && refPath[refPath.size()-1] != '\\'){
        refPath += SEPARATOR_STRING;
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

    LineBuffer_SoftClear(lb);
    LineBuffer_SoftClearReset(lb);

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
            if(opener->basePath[opener->pathLen-1] == '/' ||
               opener->basePath[opener->pathLen-1] == '\\')
            { // change dir
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
                    CODY_IGNORE(storage->Chdir(opener->basePath));
                    return 0;
                }

                opener->pathLen = n;
                p[n] = 0; // copy opener->basePath into context cwd

                QueryBar_SetEntry(queryBar, view, opener->basePath, n);
                FileOpenUpdateList(view);
                search = 0;
            }
        }else if(entry[len-1] == '/' || entry[len-1] == '\\'){ // user typed '/'
            char *content = nullptr;
            uint contentLen = 0;
            entry[len-1] = SEPARATOR_CHAR;
            QueryBar_GetWrittenContent(queryBar, &content, &contentLen);

            if(storage->Chdir(content) < 0){
                printf("Failed to change to %s directory\n", content);
                return -1;
            }

            if(ListFileEntriesAndCheckLoaded(content, &opener->entries,
                                             &opener->entryCount, &opener->entrySize) < 0)
            {
                printf("Failed to list files from %s\n", content);
                CODY_IGNORE(storage->Chdir(opener->basePath));
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
        folder[opener->pathLen + entry->pLen] = SEPARATOR_CHAR;
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
            CODY_IGNORE(storage->Chdir(opener->basePath));
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

int InteractiveListStart(View *view, char *title, OnInteractiveList onListInfo){
    AssertA(view != nullptr && title != nullptr,
            "Invalid setup of interactive start");

    LineBuffer *titlesLineBuffer = AllocatorGetN(LineBuffer, 1);
    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    uint hlen = strlen(title);

    QueryBar *queryBar = View_GetQueryBar(view);
    LineBuffer_InitBlank(titlesLineBuffer);

    int n = onListInfo(titlesLineBuffer);

    auto returnFuncCancel = [&](QueryBar *bar, View *view) -> int{
        SelectableListFreeLineBuffer(view);
        return 0;
    };

    auto returnFuncCommit = [&](QueryBar *bar, View *view) -> int{
        SelectableListFreeLineBuffer(view);
        return 1;
    };

    View_SelectableListSet(view, titlesLineBuffer, (char *)title, hlen,
                           SelectableListDefaultEntry,
                           returnFuncCancel, returnFuncCommit, &filter);

    return 0;
}

int FileOpenerCommandStart(View *view, char *basePath, ushort len,
                           OnFileOpenCallback onOpenFile)
{
    AssertA(view != nullptr && basePath != nullptr && len > 0,
            "Invalid setup of file opener");

    LineBuffer *lineBuffer = nullptr;
    char *pEntry = nullptr;
    const char *header = "Open File";
    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    uint hlen = strlen(header);
    ushort length = Min(len, PATH_MAX-1);
    QueryBar *queryBar = View_GetQueryBar(view);
    FileOpener *opener = View_GetFileOpener(view);

    Memcpy(opener->basePath, basePath, length);
    opener->basePath[length] = 0;
    opener->pathLen = length;
    opener->entryCount = 0;

    view->bufferFlags = std::vector<char>();

    if(ListFileEntriesAndCheckLoaded(basePath, &opener->entries,
                                     &opener->entryCount, &opener->entrySize) < 0)
    {
        printf("Failed to list files from %s\n", basePath);
        return -1;
    }

    pEntry = AllocatorGetN(char, len+2);
    lineBuffer = AllocatorGetN(LineBuffer, 1);
    if(basePath[len-1] != '/' && basePath[len-1] != '\\'){
        snprintf(pEntry, len+2, "%s%s", basePath, SEPARATOR_STRING);
        opener->basePath[length] = SEPARATOR_CHAR;
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
                           FileOpenCommandCommit, &filter);

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

    if(FileBufferList_FindByPath(fList, &lineBuffer, buffer->data,
                                 buffer->taken) == 0)
    {
        return 0;
    }

    BufferView_SwapBuffer(bView, lineBuffer, CodeView);
    SelectableListFreeLineBuffer(view);
    return 1;
}

int SwitchBufferCommandStart(View *view){
    std::unordered_map<std::string, int> pathMap;
    AssertA(view != nullptr, "Invalid view pointer");
    LineBuffer *lineBuffer = nullptr;
    List<FileBuffer> *list = nullptr;
    const char *header = "Switch Buffer";
    uint hlen = strlen(header);
    FileBufferList *fBuffers = FileProvider_GetBufferList();

    lineBuffer = AllocatorGetN(LineBuffer, 1);
    LineBuffer_InitBlank(lineBuffer);

    list = fBuffers->fList;
    view->bufferFlags = std::vector<char>();

    auto filler = [&](FileBuffer *buf) -> int{
        if(buf){
            if(buf->lineBuffer){
                char val = 0;
                char *ptr = buf->lineBuffer->filePath;
                uint len = buf->lineBuffer->filePathSize;
                uint n = GetSimplifiedPathName(ptr, len);
                char *simpl = &ptr[n];

                std::string name(simpl, len-n);

                LineBuffer_InsertLine(lineBuffer, ptr, len);
                if(pathMap.find(name) != pathMap.end()){
                    val = 1;
                }
                pathMap[name] = val;
                //uint n = GetSimplifiedPathName(ptr, len);
                //LineBuffer_InsertLine(lineBuffer, &ptr[n], len - n);
            }
        }

        return 1;
    };

    List_Transverse<FileBuffer>(list, filler);

    view->bufferFlags.resize(lineBuffer->lineCount);
    for(int i = 0; i < lineBuffer->lineCount; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        uint n = GetSimplifiedPathName(buffer->data, buffer->taken);
        char *simpl = &buffer->data[n];

        std::string name(simpl, buffer->taken-n);
        view->bufferFlags[i] = pathMap[name];
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;

    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           SwitchBufferCommandCommit, &filter);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Switch theme routines.
//////////////////////////////////////////////////////////////////////////////////////////

typedef struct{
    const char *name;
}FontDescription;

int SwitchFontCommandCommit(QueryBar *queryBar, View *view){
    Buffer *buffer = nullptr;
    uint active = View_SelectableListGetActiveIndex(view);
    View_SelectableListGetItem(view, active, &buffer);
    if(!buffer){
        return 0;
    }

    SwapFontInternal(std::string(buffer->data, buffer->taken));

    SelectableListFreeLineBuffer(view);
    return 1;
}

int SwitchFontCommandStart(View *view){
    AssertA(view != nullptr, "Invalid view pointer");
    LineBuffer *lineBuffer = nullptr;
    const char *header = "Switch Font";
    uint hlen = strlen(header);
    lineBuffer = AllocatorGetN(LineBuffer, 1);

    std::vector<FontDescription> fonts = {
        {"default"},
        {"consolas"},
        {"commit"},
        {"ubuntu"},
        {"dejavu"},
        {"jet"}
    };

    LineBuffer_InitBlank(lineBuffer);
    for(uint i = 0; i < fonts.size(); i++){
        FontDescription desc = fonts.at(i);
        uint slen = strlen(desc.name);
        LineBuffer_InsertLine(lineBuffer, (char *)desc.name, slen);
    }

    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
    View_SelectableListSet(view, lineBuffer, (char *)header, hlen,
                           SelectableListDefaultEntry, SelectableListDefaultCancel,
                           SwitchFontCommandCommit, &filter);
    return 0;
}

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
