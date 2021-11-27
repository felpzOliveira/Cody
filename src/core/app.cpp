#include <app.h>
#include <keyboard.h>
#include <lex.h>
#include <utilities.h>
#include <font.h>
#include <bufferview.h>
#include <x11_display.h>
#include <undo.h>
#include <view.h>
#include <string.h>
#include <file_provider.h>
#include <autocomplete.h>
#include <types.h>
#include <view_tree.h>
#include <control_cmds.h>
#include <parallel.h>
#include <timing.h>
#include <gitbase.h>
#include <gitbuffer.h>

#define DIRECTION_LEFT  0
#define DIRECTION_UP    1
#define DIRECTION_DOWN  2
#define DIRECTION_RIGHT 3

#define MAX_VIEWS 16

typedef struct{
    BindingMap *freeTypeMapping;
    BindingMap *queryBarMapping;
    BindingMap *autoCompleteMapping;
    View *activeView;
    int activeId;
    
    char cwd[PATH_MAX];
    Geometry currentGeometry;
    Float currentLineHeight;
}App;

static App appContext = { 
    .freeTypeMapping = nullptr, 
    .activeView = nullptr,
};

AppConfig appGlobalConfig;

void AppInitializeFreeTypingBindings();
void AppInitializeQueryBarBindings();
void AppCommandAutoComplete();

void AppSetViewingGeometry(Geometry geometry, Float lineHeight){
    Float w = (Float)(geometry.upper.x - geometry.lower.x);
    Float h = (Float)(geometry.upper.y - geometry.lower.y);
    auto fn = [&](ViewNode *node) -> int{
        View *view = node->view;
        if(view){
            uint x0 = (uint)(w * view->geometry.extensionX.x);
            uint x1 = (uint)(w * view->geometry.extensionX.y);
            uint y0 = (uint)(h * view->geometry.extensionY.x);
            uint y1 = (uint)(h * view->geometry.extensionY.y);

            Geometry targetGeo;
            targetGeo.lower = vec2ui(x0, y0);
            targetGeo.upper = vec2ui(x1, y1);
            targetGeo.extensionX = view->geometry.extensionX;
            targetGeo.extensionY = view->geometry.extensionY;
            View_SetGeometry(view, targetGeo, lineHeight);
        }

        return 0;
    };

    ViewTree_ForAllViews(fn);
    appContext.currentGeometry = geometry;
    appContext.currentLineHeight = lineHeight;
}

Geometry AppGetScreenGeometry(Float *lineHeight){
    *lineHeight = appContext.currentLineHeight;
    return appContext.currentGeometry;
}

void AppEarlyInitialize(){
    View *view = nullptr;

    // TODO: Configuration file
    // NOTE: Configuration must be done first as tab spacing
    //       needs to be defined before the lexer can correctly mount buffers
    appGlobalConfig.tabSpacing = 4;
    appGlobalConfig.useTabs = 0;
    appGlobalConfig.cStyle = CURSOR_RECT;
    appGlobalConfig.autoCompleteSize = 0;

    ViewTree_Initialize();
    FileProvider_Initialize();

    ViewNode *node = ViewTree_GetCurrent();
    view = node->view;

    view->geometry.lower = vec2ui();
    appContext.activeView = view;

    View_SetActive(appContext.activeView, 1);

    GetCurrentWorkingDirectory(appContext.cwd, PATH_MAX);

    appContext.autoCompleteMapping = AutoComplete_Initialize();
    std::string dir(appContext.cwd); dir += "/.cody";
    appGlobalConfig.configFolder = dir;
    appGlobalConfig.rootFolder = appContext.cwd;
    appGlobalConfig.configFile = dir + std::string("/.config");

    Git_Initialize();
    Git_OpenRootRepository();

    GitBuffer_InitializeInternalBuffer();
}

int AppGetTabConfiguration(int *using_tab){
    if(using_tab){
        *using_tab = appGlobalConfig.useTabs;
    }

    return appGlobalConfig.tabSpacing;
}

void AppAddStoredFile(std::string basePath){
    std::string fullPath = appGlobalConfig.rootFolder + std::string("/") + basePath;
    appGlobalConfig.filesStored.push_back(fullPath);
}

int AppIsStoredFile(std::string path){
    for(std::string &s : appGlobalConfig.filesStored){
        if(s == path) return 1;
    }
    return false;
}

std::string AppGetConfigFilePath(){
    return appGlobalConfig.configFile;
}

std::string AppGetConfigDirectory(){
    return std::string(appGlobalConfig.configFolder);
}

std::string AppGetRootDirectory(){
    return std::string(appGlobalConfig.rootFolder);
}

char *AppGetContextDirectory(){
    return &appContext.cwd[0];
}

View *AppGetActiveView(){
    return appContext.activeView;
}

BufferView *AppGetActiveBufferView(){
    return &appContext.activeView->bufferView;
}

void AppSetBindingsForState(ViewState state){
    switch(state){
        case View_SelectableList:
        case View_QueryBar: KeyboardSetActiveMapping(appContext.queryBarMapping); break;
        case View_FreeTyping: KeyboardSetActiveMapping(appContext.freeTypeMapping); break;
        case View_AutoComplete: KeyboardSetActiveMapping(appContext.autoCompleteMapping); break;
        default:{
            AssertErr(0, "Not implemented binding");
        }
    }
}

void AppSetActiveView(View *view){
    AssertA(view != nullptr, "Invalid view");
    if(appContext.activeView){
        View_SetActive(appContext.activeView, 0);
    }
    appContext.activeView = view;
    View_SetActive(appContext.activeView, 1);
    ViewState state = View_GetState(appContext.activeView);
    
    AppSetBindingsForState(state);
}

void AppUpdateViews(){
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View *view = iterator.value->view;

        BufferView *bView = &view->bufferView;
        BufferView_Synchronize(bView);

        ViewTree_Next(&iterator);
    }
}

void AppRestoreCurrentBufferViewState(){
    BufferView *view = AppGetActiveBufferView();
    if(view){
        ViewType type = BufferView_GetViewType(view);
        // TODO: Add as needed
        if(type == GitDiffView){
            AppCommandGitDiffCurrent();
        }
    }
}

void AppCommandFreeTypingJumpToDirection(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;

    Token *token = nullptr;
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    switch(direction){
        case DIRECTION_LEFT:{ // Move left
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int tid = BufferView_LocatePreviousCursorToken(bufferView, &token);
            if(tid >= 0){
                cursor.y = Buffer_Utf8RawPositionToPosition(buffer, token->position);
            }else{
                cursor.y = 0;
            }
            
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_UP:{ // Move Up
            if(cursor.x > 0){
                uint line = cursor.x - 1;
                int picked = 0;
                while(picked == 0 && line > 0){
                    Buffer *buffer = BufferView_GetBufferAt(bufferView, line);
                    if(Buffer_IsBlank(buffer)) picked = 1;
                    else line--;
                }
                
                cursor.y = 0;
                cursor.x = line;
                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
            }
        } break;
        case DIRECTION_DOWN:{ // Move down
            if(cursor.x < BufferView_GetLineCount(bufferView)-1){
                uint line = cursor.x + 1;
                int picked = 0;
                while(picked == 0 && line < BufferView_GetLineCount(bufferView)-1){
                    Buffer *buffer = BufferView_GetBufferAt(bufferView, line);
                    if(Buffer_IsBlank(buffer)) picked = 1;
                    else line++;
                }
                
                cursor.y = 0;
                cursor.x = line;
                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
            }
        } break;
        
        case DIRECTION_RIGHT:{ // Move right
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int tid = BufferView_LocateNextCursorToken(bufferView, &token);
            if(tid >= 0){
                cursor.y = Buffer_Utf8RawPositionToPosition(buffer,
                                    token->position + token->size);
            }else{
                cursor.y = buffer->count;
            }
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        
        default: AssertA(0, "Invalid direction given");
    }
}

void AppCommandFreeTypingArrows(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;

    uint lineCount = BufferView_GetLineCount(bufferView);
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    switch(direction){
        case DIRECTION_LEFT:{ // Move left
            if(cursor.y == 0){ // move up
                cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            }else{
                cursor.y -= 1;
            }
            
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, (uint)0, buffer->count);
        } break;
        case DIRECTION_UP:{ // Move Up
            cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
        } break;
        case DIRECTION_DOWN:{ // Move down
            cursor.x = Clamp(cursor.x+1, (uint)0, lineCount-1);
            
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
        } break;
        
        case DIRECTION_RIGHT:{ // Move right
            Buffer *buffer = LineBuffer_GetBufferAt(bufferView->lineBuffer, cursor.x);
            if(cursor.y >= buffer->count){ // move down
                cursor.x = Clamp(cursor.x+1, (uint)0, lineCount-1);
            }else{
                cursor.y += 1;
            }
            
            buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, (uint)0, buffer->count);
        } break;
        
        default: AssertA(0, "Invalid direction given");
    }

    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void RemountTokensBasedOn(BufferView *view, uint base, uint offset=0){
    LineBuffer *lineBuffer = view->lineBuffer;
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, base, offset);
}

void AppQueryBarSearchJumpToResult(QueryBar *bar, View *view){
    QueryBarCmdSearch *result = nullptr;
    BufferView *bView = View_GetBufferView(view);
    if(!bView->lineBuffer) return;
    QueryBar_GetSearchResult(bar, &result);
    
    if(result->valid){
        Buffer *buf = BufferView_GetBufferAt(bView, result->lineNo);
        // TODO: This is being called even when it is not search operation!
        if(buf){
            uint c = Buffer_Utf8RawPositionToPosition(buf, result->position);
            BufferView_CursorToPosition(bView, result->lineNo, c);
        }
    }
}

void AppDefaultRemoveOne(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    BufferView *bufferView = View_GetBufferView(view);

    if(state == View_FreeTyping || state == View_AutoComplete){
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
        
        NullRet(buffer);
        NullRet(bufferView->lineBuffer);
        NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
        Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
        SymbolTable *symTable = tokenizer->symbolTable;

        if(cursor.y > 0){
            vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
            if(id.x != (int)cursor.x || id.y != OPERATION_REMOVE_CHAR){
                // user started to edit a different buffer
                UndoRedoUndoPushInsert(&bufferView->lineBuffer->undoRedo, buffer, cursor);
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                           vec2i((int)cursor.x, OPERATION_REMOVE_CHAR));
            }

            Buffer_EraseSymbols(buffer, symTable);
            
            Buffer_RemoveRange(buffer, cursor.y-1, cursor.y);
            RemountTokensBasedOn(bufferView, cursor.x);
            cursor.y -= 1;
            
        }else if(cursor.x > 0){
            int offset = buffer->count;
            Buffer *pBuffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            Buffer_EraseSymbols(pBuffer, symTable);
            Buffer_EraseSymbols(buffer, symTable);

            LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
            LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(cursor.x, OPERATION_REMOVE_LINE));
            buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            RemountTokensBasedOn(bufferView, cursor.x-1);
            
            cursor.y = buffer->count - offset;
            cursor.x -= 1;
            UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
        }
        
        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
        BufferView_Dirty(bufferView);
        if(state == View_AutoComplete){
            /* help auto-complete better detect interrupt events since it is using
               the default entry/removal routines */
            appGlobalConfig.autoCompleteSize -= 1;
            if(appGlobalConfig.autoCompleteSize <= 0){
                AutoComplete_Interrupt();
                appGlobalConfig.autoCompleteSize = 0;
            }else{
                AppCommandAutoComplete();
            }
        }
    }else if(View_IsQueryBarActive(view)){
        QueryBar *bar = View_GetQueryBar(view);
        if(QueryBar_RemoveOne(bar, view) != 0){
            AppQueryBarSearchJumpToResult(bar, view); // TODO: other choises
        }
    }
}

void AppCommandRemovePreviousToken(){
    Token *token = nullptr;
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    int tid = BufferView_LocatePreviousCursorToken(bufferView, &token);
    if(tid >= 0){
        vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
        if(id.x != (int)cursor.x || id.y != OPERATION_REMOVE_CHAR){
            // user started to edit a different buffer
            UndoRedoUndoPushInsert(&bufferView->lineBuffer->undoRedo, buffer, cursor);
            LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                       vec2i((int)cursor.x, OPERATION_REMOVE_CHAR));
        }

        Buffer_EraseSymbols(buffer, symTable);
        uint u8tp = Buffer_Utf8RawPositionToPosition(buffer, token->position);
        Buffer_RemoveRange(buffer, u8tp, cursor.y);
        cursor.y = u8tp;
        RemountTokensBasedOn(bufferView, cursor.x);
    }else if(cursor.x > 0){
        int offset = buffer->count;
        Buffer *pBuffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
        Buffer_EraseSymbols(pBuffer, symTable);
        Buffer_EraseSymbols(buffer, symTable);

        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
        buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
        RemountTokensBasedOn(bufferView, cursor.x-1);
        
        cursor.y = buffer->count - offset;
        cursor.x -= 1;
        UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
    }
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    
    BufferView_AdjustGhostCursorIfOut(bufferView);
    BufferView_Dirty(bufferView);
}

void AppCommandInsertTab(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    char spaces[16];
    int offset = 0;
    if(appGlobalConfig.useTabs){
        Memset(spaces, '\t', appGlobalConfig.tabSpacing);
        offset = 1;
    }else{
        Memset(spaces, ' ', appGlobalConfig.tabSpacing);
        offset = appGlobalConfig.tabSpacing;
    }
    
    vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
    if(id.x != (int)cursor.x || id.y != OPERATION_INSERT_CHAR){
        // user started to edit a different buffer
        UndoRedoUndoPushRemove(&bufferView->lineBuffer->undoRedo, buffer, cursor);
        LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                   vec2i((int)cursor.x, OPERATION_INSERT_CHAR));
    }
    
    Buffer_InsertStringAt(buffer, cursor.y, spaces, appGlobalConfig.tabSpacing);
    RemountTokensBasedOn(bufferView, cursor.x);
    
    cursor.y += offset;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_Dirty(bufferView);
}

void AppCommandRemoveTextBlock(BufferView *bufferView, vec2ui start, vec2ui end){
    Buffer *buffer = BufferView_GetBufferAt(bufferView, start.x);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    if(start.x == end.x){
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, start.y, end.y);
    }else{
        uint rmov = 0;
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, start.y, buffer->count);
        for(uint i = start.x + 1; i < end.x; i++){
            Buffer *b0 = BufferView_GetBufferAt(bufferView, start.x+1);
            Buffer_EraseSymbols(b0, symTable);
            LineBuffer_RemoveLineAt(bufferView->lineBuffer, start.x+1);
            rmov++;
        }
        
        // remove end now
        buffer = BufferView_GetBufferAt(bufferView, end.x - rmov);
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, 0, end.y);
        
        // merge start and end
        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, start.x);
        BufferView_Dirty(bufferView);
    }
}

vec2ui AppCommandNewLine(BufferView *bufferView, vec2ui at){
    char *lineHelper = nullptr;
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, at.x);
    Buffer *bufferp1 = nullptr;
    uint len = 0;

    Buffer_EraseSymbols(buffer, symTable);

    uint s = AppComputeLineIndentLevel(buffer, at.y);
    uint tid = Buffer_GetTokenAt(buffer, at.y);
    len = appGlobalConfig.tabSpacing * s;

    // consider the case where the next token is a brace close and the user want to
    // ignore the last indent level
    if(tid < buffer->tokenCount){
        if(buffer->tokens[tid].identifier == TOKEN_ID_BRACE_CLOSE){
            if((int)len >= appGlobalConfig.tabSpacing){
                len -= appGlobalConfig.tabSpacing;
            }
        }
    }

    int toNextLine = Max((int)buffer->count - (int)at.y, 0);
    char *dataptr = nullptr;
    if(toNextLine > 0){
        uint p = Buffer_Utf8PositionToRawPosition(buffer, (uint)at.y, nullptr);
        dataptr = &buffer->data[p];
        toNextLine = buffer->taken - p;
    }
    
    if(len + toNextLine > 0){
        lineHelper = AllocatorGetN(char, (len + toNextLine) * sizeof(char));
    }
    
    if(len > 0){
        char indentChar = ' ';
        if(appGlobalConfig.useTabs) indentChar = '\t';
        Memset(lineHelper, indentChar, sizeof(char) * len);
    }
    
    if(toNextLine > 0){
        Memcpy(&lineHelper[len], dataptr, toNextLine * sizeof(char));
    }
    
    LineBuffer_InsertLineAt(lineBuffer, at.x+1, lineHelper, len+toNextLine, 0);
    if(toNextLine > 0){
        buffer = BufferView_GetBufferAt(bufferView, at.x);
        Buffer_RemoveRange(buffer, at.y, buffer->count);
    }
    
    RemountTokensBasedOn(bufferView, at.x, 2);
    buffer = BufferView_GetBufferAt(bufferView, at.x);
    
    AllocatorFree(lineHelper);
    
    // get the buffer again in case it was end of a file
    bufferp1 = BufferView_GetBufferAt(bufferView, at.x+1);
    
    Buffer_Claim(buffer);
    Buffer_Claim(bufferp1);
    at.x += 1;
    at.y = Buffer_Utf8RawPositionToPosition(bufferp1, len);
    return at;
}

void AppCommandNewLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    NullRet(buffer);
    
    UndoRedoUndoPushMerge(&bufferView->lineBuffer->undoRedo, buffer, cursor);
    
    cursor = AppCommandNewLine(bufferView, cursor);
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_AdjustGhostCursorIfOut(bufferView);
    LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(cursor.x-1, OPERATION_INSERT_LINE));
    BufferView_Dirty(bufferView);
}

//TODO
void AppCommandRedo(){
    
}

void AppCommandKillEndOfLine(){
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView);
    NullRet(bView->lineBuffer);
    NullRet(LineBuffer_IsWrittable(bView->lineBuffer));

    vec2ui cursor = BufferView_GetCursorPosition(bView);
    Buffer *buffer = BufferView_GetBufferAt(bView, cursor.x);
    NullRet(buffer);

    if(buffer->taken > 0){
        uint p = Buffer_Utf8RawPositionToPosition(buffer, cursor.y);
        UndoRedoUndoPushInsert(&bView->lineBuffer->undoRedo, buffer, cursor);

        Buffer_RemoveRangeRaw(buffer, p, buffer->taken);
        Buffer_Claim(buffer);
    }else{
        uint oc = cursor.x;
        AppDefaultRemoveOne(); // re-use erase 
        vec2ui after = BufferView_GetCursorPosition(bView);
        if(oc != after.x && oc < bView->lineBuffer->lineCount){
            BufferView_CursorToPosition(bView, oc, 0);
            BufferView_AdjustGhostCursorIfOut(bView);
        }
    }

    RemountTokensBasedOn(bView, cursor.x-1);
}

void AppCommandKillBuffer(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView);
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

    LineBuffer *lineBuffer = bufferView->lineBuffer;
    if(lineBuffer){
        ViewTreeIterator iterator;
        ViewTree_Begin(&iterator);

        Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
        SymbolTable *symTable = tokenizer->symbolTable;
        for(uint i = 0; i < lineBuffer->lineCount; i++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
            Buffer_EraseSymbols(buffer, symTable);
        }

        while(iterator.value){
            View *view = iterator.value->view;
            BufferView *bView = &view->bufferView;
            if(bView->lineBuffer == lineBuffer){
                BufferView_SwapBuffer(bView, nullptr, EmptyView);
            }
            ViewTree_Next(&iterator);
        }
        
        char *ptr = lineBuffer->filePath;
        uint pSize = lineBuffer->filePathSize;
        FileProvider_Remove(ptr, pSize);
        BufferViewLocation_RemoveLineBuffer(lineBuffer);
        LineBuffer_Free(lineBuffer);
        AllocatorFree(lineBuffer);
    }
}

void AppCommandKillView(){
    BufferView *bView = AppGetActiveBufferView();
    // we need to make sure the interface is not expanded otherwise
    // UI will get weird.
    ControlCommands_RestoreExpand();

    AppCommandKillBuffer();
    BufferViewLocation_RemoveView(bView);
     // TODO: Check if it is safe to erase the view in the vnode here
    ViewNode *vnode = ViewTree_DestroyCurrent(1);
    AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
    appContext.activeView = vnode->view;
    AppSetActiveView(appContext.activeView);
}

void AppCommandUndo(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

    LineBuffer *lineBuffer = bufferView->lineBuffer;
    BufferChange *bChange = UndoRedoGetNextUndo(&lineBuffer->undoRedo);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    if(bChange){
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        //TODO: Update symbol table on commands that require it
        switch(bChange->change){
            case CHANGE_NEWLINE:{
                cursor = AppCommandNewLine(bufferView, bChange->bufferInfo);
                RemountTokensBasedOn(bufferView, cursor.x);
                UndoRedoPopUndo(&lineBuffer->undoRedo);
            } break;
            
            /* These cases rely on replacing the original line with a saved one */
            case CHANGE_MERGE:
            case CHANGE_REMOVE:
            case CHANGE_INSERT:{
                Buffer *b = bChange->buffer;
                TokenizerStateContext *context = &b->stateContext;
                
                uint fTrack = context->forwardTrack;
                uint bTrack = context->backTrack;
                
                Buffer *buffer = LineBuffer_ReplaceBufferAt(bufferView->lineBuffer, b,
                                                            bChange->bufferInfo.x);
                
                if(bChange->change == CHANGE_MERGE){
                    /* In case of a merge we need to also remove the following line */
                    Buffer *bp1 = BufferView_GetBufferAt(bufferView, bChange->bufferInfo.x+1);
                    Buffer_EraseSymbols(bp1, symTable);
                    LineBuffer_RemoveLineAt(bufferView->lineBuffer,
                                            bChange->bufferInfo.x+1);
                }
                
                context = &buffer->stateContext;
                fTrack = Max(fTrack, context->forwardTrack);
                bTrack = Max(bTrack, context->backTrack);
                Buffer_EraseSymbols(buffer, symTable);

                UndoRedoPopUndo(&lineBuffer->undoRedo);
                // give ownership of the buffer to the undo system
                // this reduces memory allocations/frees
                UndoSystemTakeBuffer(buffer);
                
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                uint base = cursor.x > bTrack ? cursor.x - bTrack : 0;
                RemountTokensBasedOn(bufferView, base, fTrack);
                // restore the active buffer and let stack grow again
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i((int)cursor.x,-1));
            } break;
            
            case CHANGE_BLOCK_REMOVE:{
                vec2ui start = bChange->bufferInfo;
                vec2ui end   = bChange->bufferInfoEnd;
                cursor = start;
                
                //printf("Removing range [%u %u] x [%u %u]\n",
                //       start.x, start.y, end.x, end.y); 
                
                AppCommandRemoveTextBlock(bufferView, start, end);
                
                RemountTokensBasedOn(bufferView, cursor.x, 1);
                UndoRedoPopUndo(&lineBuffer->undoRedo);
                
            } break;
            case CHANGE_BLOCK_INSERT:{
                vec2ui start = bChange->bufferInfo;
                char *text = bChange->text;
                uint size = bChange->size;
                if(text && size > 0){
                    uint off = 0;
                    uint startBuffer = start.x;
                    uint n = LineBuffer_InsertRawTextAt(bufferView->lineBuffer, text, size, 
                                                        start.x, start.y, &off);
                    
                    uint endx = start.x + n;
                    uint endy = off;
                    LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(endx, -1));
                    RemountTokensBasedOn(bufferView, startBuffer, n);
                    cursor.x = endx;
                    cursor.y = endy;
                    UndoRedoPopUndo(&lineBuffer->undoRedo);
                    AllocatorFree(text);
                }
            } break;
            default:{ printf("Unknow undo command\n"); }
        }
        
        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
    }
}

void AppCommandHomeLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);

    if(buffer == nullptr){
        cursor.y = 0;
    }else if(Buffer_IsBlank(buffer)){
        cursor.y = 0;
    }else{
        uint tid = Buffer_FindFirstNonEmptyToken(buffer);
        Token *token = &buffer->tokens[tid];
        uint p8 = Buffer_Utf8RawPositionToPosition(buffer, token->position);
        if(cursor.y <= p8) cursor.y = 0;
        else{
            cursor.y = p8;
        }
    }

    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppCommandQueryBarSearchAndReplace(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->bufferView.lineBuffer));
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBarCmdSearchAndReplace *replace = &bar->replaceCmd;
        replace->state = QUERY_BAR_SEARCH_AND_REPLACE_SEARCH;

        auto onConfirm = [&](QueryBar *qbar, View *vview, int accepted) -> int{
            // The search and replace command uses the search command under the hood
            // so you need to get that result too.
            QueryBarCmdSearch *searchResult = &qbar->searchCmd;
            QueryBarCmdSearchAndReplace *searchReplace = &qbar->replaceCmd;
            if(accepted){
                BufferView *bView = View_GetBufferView(vview);
                Buffer *buf = BufferView_GetBufferAt(bView, searchResult->lineNo);
                if(buf){
                    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bView->lineBuffer);
                    SymbolTable *symTable = tokenizer->symbolTable;
                    vec2ui cursor = BufferView_GetCursorPosition(bView);

                    UndoRedoUndoPushInsert(&bView->lineBuffer->undoRedo, buf, cursor);

                    Buffer_EraseSymbols(buf, symTable);

                    Buffer_RemoveRangeRaw(buf, searchResult->position,
                                          searchResult->position + searchReplace->toLocateLen);
                    Buffer_InsertRawStringAt(buf, searchResult->position,
                                             searchReplace->toReplace, searchReplace->toReplaceLen, 0);
                    RemountTokensBasedOn(bView, cursor.x);
                    BufferView_Dirty(bView);
                }
            }
            return 0;
        };

        QueryBar_SetInteractiveSearchCallback(bar, onConfirm);
        QueryBar_Activate(bar, QUERY_BAR_CMD_SEARCH_AND_REPLACE, view);
        View_ReturnToState(view, View_QueryBar);
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

bool AppIsPathFromRoot(std::string path){
    std::string rootDir = AppGetRootDirectory();
    return StringStartsWith((char *)path.c_str(), path.size(),
                            (char *)rootDir.c_str(), rootDir.size());
}

void AppCommandQueryBarInteractiveCommand(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    if(state != View_QueryBar){
        QueryBar *qbar = View_GetQueryBar(view);

        auto emptyFunc = [&](QueryBar *bar, View *view) -> int{ return 0; };
        auto onCommit = [&](QueryBar *bar, View *view) -> int{
            char *content = nullptr;
            uint size = 0;
            QueryBar_GetWrittenContent(bar, &content, &size);
            int r = BaseCommand_Interpret(content, size, view);
            return r == 2 ? 0 : -1;
        };

        QueryBar_ActivateCustom(qbar, nullptr, 0, emptyFunc,
                                emptyFunc, onCommit, nullptr);

        View_ReturnToState(view, View_QueryBar);
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandStoreFileCommand(){
    View *view = AppGetActiveView();
    NullRet(view->bufferView.lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->bufferView.lineBuffer));
    std::string writtenPath = std::string(view->bufferView.lineBuffer->filePath);
    std::string rootDir = AppGetRootDirectory();
    if(AppIsPathFromRoot(writtenPath)){
        writtenPath = std::string(&view->bufferView.lineBuffer->filePath[rootDir.size()+1]);
    }else{
        printf("Warning: linebuffer has inconsistent path\n");
        printf("File Path : %s\n", view->bufferView.lineBuffer->filePath);
        return;
    }

    int r = BaseCommand_AddFileEntryIntoAutoLoader((char *)writtenPath.c_str(),
                                                   writtenPath.size());
    if(r == 0){
        AppAddStoredFile(writtenPath);
    }
}

void AppCommandQueryBarSearch(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_SEARCH, view);
        View_ReturnToState(view, View_QueryBar);
        
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandQueryBarGotoLine(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_JUMP_TO_LINE, view);
        View_ReturnToState(view, View_QueryBar);
        
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandQueryBarRevSearch(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_REVERSE_SEARCH, view);
        View_ReturnToState(view, View_QueryBar);
        
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandEndLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = buffer->count;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

ViewNode *AppGetNextViewNode(){
    int n = 0;
    View *view = appContext.activeView;
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);

    ViewNode *vnode = iterator.value;
    while(iterator.value){
        if(n == 1){
            vnode = iterator.value;
            break;
        }

        if(view == iterator.value->view){
            n = 1;
        }

        ViewTree_Next(&iterator);
    }

    return vnode;
}

void AppCommandSwapView(){
    ViewNode *vnode = AppGetNextViewNode();
    if(vnode->view){
        if(BufferView_IsVisible(&vnode->view->bufferView)){
            ViewTree_SetActive(vnode);
            AppSetActiveView(vnode->view);
        }
    }
}

void AppCommandSetGhostCursor(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_GhostCursorFollow(bufferView);
}

void AppCommandSwapLineNbs(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_ToogleLineNumbers(bufferView);
}

void AppCommandAutoComplete(){
    View *view = AppGetActiveView();
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView);
    NullRet(bView->lineBuffer);
    NullRet(LineBuffer_IsWrittable(bView->lineBuffer));

    vec2ui cursor = BufferView_GetCursorPosition(bView);
    Buffer *buffer = BufferView_GetBufferAt(bView, cursor.x);
    NullRet(buffer);

    //TODO: Comments cannot find their token id
    uint tid = Buffer_GetTokenAt(buffer, cursor.y > 0 ? cursor.y - 1 : 0);
    if(buffer->tokenCount > 0){
        if(tid <= buffer->tokenCount-1){
            Token *token = &buffer->tokens[tid];
            if(token->identifier != TOKEN_ID_SPACE){
                SelectableList *list = view->autoCompleteList;
                char *ptr = &buffer->data[token->position];
                if(View_GetState(view) != View_AutoComplete){
                    appGlobalConfig.autoCompleteSize = (int)token->size;
                }

                int n = AutoComplete_Search(ptr, token->size, list);
                if(n > 0 && View_GetState(view) != View_AutoComplete){
                    View_ReturnToState(view, View_AutoComplete);
                    AppSetBindingsForState(View_AutoComplete);
                    SelectableList_ResetView(list);
                    if(n == 1){
                        AutoComplete_Commit();
                        appGlobalConfig.autoCompleteSize = 0;
                    }
                }
            }
        }
    }
}

void AppCommandLineQuicklyDisplay(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_StartNumbersShowTransition(bufferView, 3.0);
}

void AppCommandSaveBufferView(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    LineBuffer_SaveToStorage(bufferView->lineBuffer);
    bufferView->lineBuffer->is_dirty = 0;
}

void AppCommandCopy(){
    vec2ui start, end;
    char *ptr = nullptr;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        uint size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetStringX11(ptr, size);
        //printf("Copied %s === last byte: %d -- size: %u\n", ptr, (int)ptr[size], size);
    }
}

void AppCommandJumpNesting(){
    DoubleCursor *cursor = nullptr;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    BufferView_GetCursor(view, &cursor);
    
    if(BufferView_CursorNestIsValid(view)){
        //TODO: Create toogle
        if(view->activeNestPoint == -1){
            view->activeNestPoint = 0;
        }else{
            view->activeNestPoint = 1 - view->activeNestPoint;
        }
        
        vec2ui p = view->activeNestPoint == 0 ? cursor->nestStart : cursor->nestEnd;
        Buffer *buffer = BufferView_GetBufferAt(view, p.x);
        uint pos = Buffer_Utf8RawPositionToPosition(buffer, buffer->tokens[p.y].position);
        BufferView_CursorToPosition(view, p.x, pos);
    }
}

void AppCommandCut(){
    vec2ui start, end;
    uint size = 0;
    char *ptr = nullptr;
    BufferView *view = AppGetActiveBufferView();
    
    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));

    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetStringX11(ptr, size);
        //printf("Cut %s === last byte: %d -- size: %u\n", ptr, (int)ptr[size], size);
        
        UndoRedoUndoPushInsertBlock(&view->lineBuffer->undoRedo, start, ptr, size);
        AppCommandRemoveTextBlock(view, start, end);
        
        RemountTokensBasedOn(view, start.x, 1);
        BufferView_CursorToPosition(view, start.x, start.y);
        BufferView_AdjustGhostCursorIfOut(view);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppPasteString(const char *p, uint size){
    BufferView *view = AppGetActiveBufferView();
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(view->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));

    if(size > 0 && p){
        CopySection section;
        Buffer *buffer = nullptr;
        uint off = 0;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        uint startBuffer = cursor.x;

        buffer = BufferView_GetBufferAt(view, cursor.x);

        Buffer_EraseSymbols(buffer, symTable);

        uint n = LineBuffer_InsertRawTextAt(view->lineBuffer, (char *) p, size,
                                            cursor.x, cursor.y, &off);

        section = {
            .start = cursor,
            .end = vec2ui(cursor.x + n, off),
            .currTime = 0,
            .interval = kTransitionCopyFadeIn,
            .active = 1,
        };

        uint endx = cursor.x + n;
        uint endy = off;

        UndoRedoUndoPushRemoveBlock(&view->lineBuffer->undoRedo,
                                    cursor, vec2ui(endx, endy));
        LineBuffer_SetActiveBuffer(view->lineBuffer, vec2i(endx, -1), 0);
        LineBuffer_SetCopySection(view->lineBuffer, section);

        RemountTokensBasedOn(view, startBuffer, n);

        Buffer *b = LineBuffer_GetBufferAt(view->lineBuffer, endx);

        if(b == nullptr){
            BUG();
            printf("Cursor line is nullptr\n");
        }

        cursor.x = endx;
        cursor.y = Clamp(endy, (uint)0, b->count);
        BufferView_CursorToPosition(view, cursor.x, cursor.y);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppCommandPaste(){
    uint size = 0;
    const char *p = ClipboardGetStringX11(&size);
    AppPasteString(p, size);
}

uint AppComputeLineIndentLevel(Buffer *buffer, uint p){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint tid = Min(Buffer_GetTokenAt(buffer, p), buffer->tokenCount);
    uint l = ctx->indentLevel;
    for(uint i = 0; i < tid; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier == TOKEN_ID_BRACE_CLOSE){
            l = l > 0 ? l - 1 : 0;
        }else if(token->identifier == TOKEN_ID_BRACE_OPEN){
            l++;
        }
    }
    return l;
}

uint AppComputeLineLastIndentLevel(Buffer *buffer){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint l = ctx->indentLevel;
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier == TOKEN_ID_BRACE_CLOSE){
            l = l > 0 ? l - 1 : 0;
        }else if(token->identifier == TOKEN_ID_BRACE_OPEN){
            l++;
        }
    }
    
    return l;
}

uint AppComputeLineIndentLevel(Buffer *buffer){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint l = ctx->indentLevel;
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier != TOKEN_ID_SPACE){
            if(token->identifier == TOKEN_ID_BRACE_CLOSE){
                l = l > 0 ? l - 1 : 0;
            }
            
            break;
        }
    }
    
    return l;
}

void AppCommandIndentRegion(BufferView *view, vec2ui start, vec2ui end){
    char *lineHelper = nullptr;
    uint tabSize = appGlobalConfig.tabSpacing;
    char indentChar = ' ';
    int changes = 0;
    if(appGlobalConfig.useTabs){
        indentChar = '\t';
    }
    
    // Grab the maximum line size in this range
    uint maxSize = 0;
    for(uint i = start.x; i < end.x; i++){
        Buffer *buffer = BufferView_GetBufferAt(view, i);
        maxSize = Max(buffer->taken, maxSize);
    }
    
    lineHelper = AllocatorGetN(char, maxSize);
    for(uint i = start.x; i < end.x; i++){
        Buffer *buffer = BufferView_GetBufferAt(view, i);
        TokenizerStateContext *ctx = &buffer->stateContext;
        // 1- Remove all empty tokens at start of the buffer,
        //    this is usually 1 because our lex groups spaces
        //    but just in case loop this thing.
        uint targetP = 0;
        uint foundNonSpace = 0;
        int indentStyle = 0;
        uint indentLevel = ctx->indentLevel;
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(token->identifier != TOKEN_ID_SPACE){
                targetP = token->position;
                foundNonSpace = 1;
                //TODO: Is this sufficient to detect ending of a nest level?
                if(token->identifier == TOKEN_ID_BRACE_CLOSE){
                    indentLevel = indentLevel > 0 ? indentLevel - 1 : 0;
                }else if(token->identifier == TOKEN_ID_PREPROCESSOR){
                    indentStyle = 1;
                }else if(token->identifier == TOKEN_ID_COMMENT){
                    indentStyle = 2;
                }
                break;
            }
        }
        
        // 2- Add the indentation sequence the amount of times required
        uint len = indentLevel * tabSize;
        uint llen = buffer->taken - targetP;
        
        // Skip lines made entirely of spaces
        if(!foundNonSpace){
            llen = 0;
            len = 0;
        }
        
        // if this line is a preprocessor align to edge of file
        if(indentStyle == 1){
            len = 0;
        }
        
        // don't attempt to configure user comment format
        if(indentStyle == 2){
            // however if we don't allow indentation until first
            // token we would keep the tab/space as was defined
            len = targetP;
        }
        
        if(len+llen > maxSize){
            uint osize = maxSize;
            maxSize = len + llen;
            lineHelper = AllocatorExpand(char, lineHelper, maxSize, osize);
        }
        
        // insert spacing only when there is actually a token
        if(len > 0 && foundNonSpace)
            Memset(lineHelper, indentChar, len * sizeof(char));
        // insert rest of the line
        if(llen > 0)
            Memcpy(&lineHelper[len], &buffer->data[targetP], llen * sizeof(char));
        
        //TODO: Maybe we can batch all lines as a CHANGE_BLOCK_INSERT for simpler undo,
        // this undo is currently broken and I'm not sure why.
        // Save undo - redo stuff as simple inserts
        //UndoRedoUndoPushInsert(&view->lineBuffer->undoRedo, buffer, vec2ui(i, 0));
        
        // Re-insert the line in raw mode to avoid processing
        Buffer_RemoveRange(buffer, 0, buffer->count);
        if(len + llen > 0)
            Buffer_InsertRawStringAt(buffer, 0, lineHelper, len+llen, 0);
        changes = 1;
    }
    
    if(changes){ // unfortunatelly we need to retokenize to adjust tokens offsets
        RemountTokensBasedOn(view, start.x, end.x - start.x);
    }
    
    AllocatorFree(lineHelper);
}

void AppCommandIndent(){
    vec2ui start, end;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        AppCommandIndentRegion(view, start, end);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppCommandSplitHorizontal(){
    ViewTree_SplitHorizontal();
    AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
    View_SetActive(appContext.activeView, 1);
}

void AppCommandSplitVertical(){
    ViewTree_SplitVertical();
    AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
    View_SetActive(appContext.activeView, 1);
}

void AppDefaultEntry(char *utf8Data, int utf8Size){
    if(utf8Size > 0){
        int off = 0;
        int cp = StringToCodepoint(utf8Data, utf8Size, &off);
        if(Font_SupportsCodepoint(cp)){
            View *view = AppGetActiveView();
            ViewState state = View_GetState(view);
            if(state == View_FreeTyping || state == View_AutoComplete){
                BufferView *bufferView = AppGetActiveBufferView();
                NullRet(bufferView);
                NullRet(bufferView->lineBuffer);
                NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

                Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
		        SymbolTable *symTable = tokenizer->symbolTable;

                vec2ui cursor = BufferView_GetCursorPosition(bufferView);
                Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
                
                NullRet(buffer);
                
                vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
                if(id.x != (int)cursor.x || id.y != OPERATION_INSERT_CHAR){
                    // user started to edit a different buffer
                    UndoRedoUndoPushRemove(&bufferView->lineBuffer->undoRedo,
                                           buffer, cursor);
                    
                    LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                               vec2i((int)cursor.x, OPERATION_INSERT_CHAR));
                }

                //TODO: In case this thing is a lexical context, i.e.: '{', '}', ...
                //      we need to go further to update the logical identation level

                uint startX = cursor.x;
                uint offset = 0;
                
                Buffer_EraseSymbols(buffer, symTable);
                Buffer_InsertStringAt(buffer, cursor.y, utf8Data, utf8Size);

                if(utf8Size == 1){ // TODO: make this better
                    if(*utf8Data == '}' || *utf8Data == '{'){
                        if(BufferView_CursorNestIsValid(bufferView)){
                            NestPoint start = bufferView->startNest[0];
                            NestPoint end   = bufferView->endNest[start.comp];
                            startX = start.position.x;
                            uint endX = end.position.x;
                            offset = endX - startX;
                        }
                    }
                }

                Buffer_Claim(buffer);
                RemountTokensBasedOn(bufferView, startX, offset);
                
                cursor.y += StringComputeU8Count(utf8Data, utf8Size);

                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
                bufferView->lineBuffer->is_dirty = 1;

                if(state == View_AutoComplete){
                    if(utf8Size == 1 && AUTOCOMPLETE_TERMINATOR(*utf8Data)){
                        AutoComplete_Interrupt();
                    }else{
                        AppCommandAutoComplete();
                    }
                }

            }else if(View_IsQueryBarActive(view)){
                QueryBar *bar = View_GetQueryBar(view);
                int r = QueryBar_AddEntry(bar, view, utf8Data, utf8Size);
                if(r == 1){
                    AppQueryBarSearchJumpToResult(bar, view);
                }else if(r == -2){
                    AppDefaultReturn();
                }
            }
        }
    }
}

View *AppGetViewAt(int x, int y){
    ViewTreeIterator iterator;
    View *rView = nullptr, *view = nullptr;
    vec2ui r(x, y);
    ViewTree_Begin(&iterator);
    while(iterator.value){
        view = iterator.value->view;
        if(Geometry_IsPointInside(&view->geometry, r)){
            rView = view;
            break;
        }

        ViewTree_Next(&iterator);
    }
    
    return rView;
}

vec2ui AppActivateViewAt(int x, int y){
    // Check which view is the mouse on and activate it
    vec2ui r(x, y);
    View *view = nullptr;
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);
    while(iterator.value){
        view = iterator.value->view;
        if(Geometry_IsPointInside(&view->geometry, r)){
            // Activate the view and re-map position
            ViewTree_SetActive(iterator.value);
            AppSetActiveView(view);
            r = r - view->geometry.lower;
            break;
        }

        ViewTree_Next(&iterator);
    }
    
    return r;
}

void AppCommandGitStatus(){
    AppRestoreCurrentBufferViewState();
    BufferView *bView = AppGetActiveBufferView();
    std::vector<std::string> data;
    if(Git_FetchStatus(&data)){
        LineBuffer *gitbuf = GitBuffer_GetLineBuffer();
        GitBuffer_Clear();

        for(std::string v : data){
            GitBuffer_PushLine((char *)v.c_str(), v.size());
        }

        BufferView_SwapBuffer(bView, gitbuf, GitStatusView);
    }
}

void AppCommandGitDiffCurrent(){
    BufferView *bView = AppGetActiveBufferView();
    bool allow_write = true;
    NullRet(bView->lineBuffer);

    // only compute diffs for actual editable linebuffers.
    NullRet(BufferView_GetViewType(bView) == CodeView ||
            BufferView_GetViewType(bView) == GitDiffView);

    std::vector<GitDiffLine> *dif = LineBuffer_GetDiffPtr(bView->lineBuffer, 0);
    if(dif){
        vec2ui range;
        if(dif->size() > 0){
            std::vector<vec2ui> *ptr = &bView->lineBuffer->props.diffLines;
            LineBuffer_EraseDiffContent(bView->lineBuffer);
            range = vec2ui(ptr->at(0).x, ptr->at(ptr->size()-1).x);
            dif->clear();
            // removed the diff, make the state code again
            BufferView_SetViewType(bView, CodeView);
        }else{
            dif->clear();
            allow_write = false;
            std::string path = AppGetRootDirectory();
            std::string target = LineBuffer_GetStoragePath(bView->lineBuffer);
            std::string gitName = target;
            if(StringStartsWith((char *)target.c_str(), target.size(),
                                (char *)path.c_str(), path.size()))
            {
                gitName = target.substr(path.size()+1);
            }

            if(!Git_ComputeCurrentDiff()) return;
            if(!Git_FetchDiffFor((char *)gitName.c_str(), dif)) return;

            LineBuffer_InsertDiffContent(bView->lineBuffer, range);
            BufferView_SetViewType(bView, GitDiffView);
        }

        if(range.x <= range.y){
            RemountTokensBasedOn(bView, range.x, range.y - range.x);
            LineBuffer_SetWrittable(bView->lineBuffer, allow_write);
        }
    }
}

void AppCommandInsertSymbol(){
    vec2ui start, end;
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView->lineBuffer);

    if(BufferView_GetCursorSelectionRange(bView, &start, &end)){
        if(start.x == end.x){
            Buffer *buffer = BufferView_GetBufferAt(bView, start.x);
            NullRet(buffer);

            // only do this if the token is either one or a subrange of one token
            uint tid1 = Buffer_GetTokenAt(buffer, start.y);
            uint tid2 = Buffer_GetTokenAt(buffer, end.y > 0 ? end.y - 1 : 0);

            if(tid1 == tid2 && tid1 < buffer->tokenCount){
                // grab content
                uint p0 = Buffer_Utf8PositionToRawPosition(buffer, start.y);
                uint p1 = Buffer_Utf8PositionToRawPosition(buffer, end.y);

                if(p1 > p0){
                    char *ptr = &buffer->data[buffer->tokens[tid1].position];
                    SymbolTable *symTable = FileProvider_GetSymbolTable();
                    SymbolTable_Insert(symTable, ptr, p1 - p0,
                                       TOKEN_ID_DATATYPE_USER_DATATYPE);
                }
            }
        }
    }
}

void AppCommandSwapToBuildBuffer(){
    View *view = AppGetActiveView();
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
}

void AppCommandQueryBarLeftArrow(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_MoveLeft(bar);
}

void AppCommandQueryBarRightArrow(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_MoveRight(bar);
}

void AppCommandQueryBarNext(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    if(View_NextItem(view) != 0){
        AppQueryBarSearchJumpToResult(bar, view);
    }
}

void AppCommandQueryBarPrevious(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    if(View_PreviousItem(view) != 0){
        AppQueryBarSearchJumpToResult(bar, view);
    }
}

void AppDefaultReturn(){
    ViewState state = View_GetDefaultState();
    View *view = AppGetActiveView();
    if(View_GetState(view) != state){
        View_ReturnToState(view, state);
        AppSetBindingsForState(state);
    }
}

void AppCommandQueryBarCommit(){
    ViewState state = View_GetDefaultState();
    View *view = AppGetActiveView();
    if(View_GetState(view) != state){
        if(View_CommitToState(view, state) != 0){
            AppSetBindingsForState(state);
        }
    }
}

void AppCommandSwitchTheme(){
    View *view = AppGetActiveView();
    int r = SwitchThemeCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppCommandSwitchBuffer(){
    AppRestoreCurrentBufferViewState();
    View *view = AppGetActiveView();
    int r = SwitchBufferCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppCommandOpenFile(){
    AppRestoreCurrentBufferViewState();
    View *view = AppGetActiveView();
    auto fileOpen = [](View *view, FileEntry *entry) -> void{
        char targetPath[PATH_MAX];
        Tokenizer *tokenizer = nullptr;
        LineBuffer *lBuffer = nullptr;
        BufferView *bView = View_GetBufferView(view);
        if(entry){
            FileOpener *opener = View_GetFileOpener(view);
            uint l = snprintf(targetPath, PATH_MAX, "%s%s", opener->basePath, entry->path);
            if(entry->isLoaded == 0){
                FileProvider_Load(targetPath, l, &lBuffer, false);
                BufferView_SwapBuffer(bView, lBuffer, CodeView);
            }else{
                int f = FileProvider_FindByPath(&lBuffer, targetPath, l, &tokenizer);
                if(f == 0 || lBuffer == nullptr){
                    printf("Did not find buffer %s\n", entry->path);
                }else{
                    //printf("Swapped to %s\n", entry->path);
                    BufferView_SwapBuffer(bView, lBuffer, CodeView);
                }
            }
        }else{ // is file creation (or dir?)
            //TODO: Directories?
            char *content = nullptr;
            uint len = 0;
            QueryBar *querybar = View_GetQueryBar(view);
            QueryBar_GetWrittenContent(querybar, &content, &len);
            if(len > 0){
                //printf("Creating file %s\n", content);
                FileProvider_CreateFile(content, len, &lBuffer, &tokenizer);
                BufferView_SwapBuffer(bView, lBuffer, CodeView);
            }
        }
    };
    
    int r = FileOpenerCommandStart(view, appContext.cwd, 
                                   strlen(appContext.cwd), fileOpen);
    if(r >= 0){
        AppSetBindingsForState(View_SelectableList);
    }
}

void AppInitialize(){
    KeyboardInitMappings();
    AppInitializeFreeTypingBindings();
    AppInitializeQueryBarBindings();
    
    KeyboardSetActiveMapping(appContext.freeTypeMapping);
}

void AppInitializeQueryBarBindings(){
    BindingMap *mapping = nullptr;
    mapping = KeyboardCreateMapping();
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppDefaultRemoveOne, Key_Backspace);
    
    RegisterRepeatableEvent(mapping, AppCommandQueryBarNext, Key_Down);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarPrevious, Key_Up);
    
    //RegisterRepeatableEvent(mapping, AppCommandQueryBarRightArrow, Key_Right);
    //RegisterRepeatableEvent(mapping, AppCommandQueryBarLeftArrow, Key_Left);
    
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_LeftAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_RightAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarCommit, Key_Enter);
    
    appContext.queryBarMapping = mapping;
}

void AppMemoryDebugFreeze(){
#if defined(MEMORY_DEBUG)
    __memory_freeze();
    printf(" === === === FREEZED STATE\n");
#endif
}

void AppMemoryDebugCmp(){
#if defined(MEMORY_DEBUG)
    __memory_compare_state();
    getchar();
#endif
}

void AppInitializeFreeTypingBindings(){
    BindingMap *mapping = nullptr;
    mapping = KeyboardCreateMapping();
    
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppDefaultRemoveOne, Key_Backspace);

    //DEBUG KEYS
    RegisterRepeatableEvent(mapping, AppMemoryDebugFreeze, Key_LeftControl, Key_1);
    RegisterRepeatableEvent(mapping, AppMemoryDebugCmp, Key_LeftControl, Key_2);

    //TODO: Create basic mappings
    RegisterRepeatableEvent(mapping, AppCommandLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppCommandRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandUpArrow, Key_Up);
    RegisterRepeatableEvent(mapping, AppCommandDownArrow, Key_Down);

    RegisterRepeatableEvent(mapping, AppCommandSwitchTheme, Key_LeftControl, Key_T);
    
    RegisterRepeatableEvent(mapping, AppCommandOpenFile, Key_LeftAlt, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftControl);
    
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftAlt);
    
    RegisterRepeatableEvent(mapping, AppCommandRemovePreviousToken,
                            Key_Backspace, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandRemovePreviousToken,
                            Key_Backspace, Key_LeftAlt);
    
    RegisterRepeatableEvent(mapping, AppCommandNewLine, Key_Enter);
    
    RegisterRepeatableEvent(mapping, AppCommandHomeLine, Key_Home);
    RegisterRepeatableEvent(mapping, AppCommandEndLine, Key_End);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_LeftAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_RightAlt, Key_W);
    
    RegisterRepeatableEvent(mapping, AppCommandSetGhostCursor, Key_Space,
                            Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandSetGhostCursor, Key_Space,
                            Key_RightControl);
    
    RegisterRepeatableEvent(mapping, AppCommandAutoComplete, Key_LeftControl, Key_N);
    RegisterRepeatableEvent(mapping, AppCommandSwapLineNbs, Key_LeftAlt, Key_N);
    RegisterRepeatableEvent(mapping, AppCommandKillBuffer, Key_LeftControl, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandKillEndOfLine, Key_LeftAlt, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandKillView, Key_LeftControl, Key_LeftAlt, Key_K);

    RegisterRepeatableEvent(mapping, AppCommandSaveBufferView, Key_LeftAlt, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarSearch, Key_LeftControl, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarSearchAndReplace, Key_LeftControl, Key_O);
    RegisterRepeatableEvent(mapping, AppCommandGitDiffCurrent, Key_LeftAlt, Key_O);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftControl, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftAlt, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarRevSearch, Key_LeftControl, Key_R);

    RegisterRepeatableEvent(mapping, AppCommandSwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, AppCommandInsertTab, Key_Tab);

    RegisterRepeatableEvent(mapping, AppCommandSplitHorizontal, Key_LeftAlt, Key_LeftControl, Key_H);
    RegisterRepeatableEvent(mapping, AppCommandSplitVertical, Key_LeftAlt, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandPaste, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_C);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_Q);
    RegisterRepeatableEvent(mapping, AppCommandSwapToBuildBuffer, Key_LeftControl, Key_M);
    
    RegisterRepeatableEvent(mapping, AppCommandUndo, Key_LeftControl, Key_Z);

    //TODO: re-do (AppCommandRedo)
    RegisterRepeatableEvent(mapping, AppCommandStoreFileCommand, Key_LeftControl, Key_E);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarInteractiveCommand, Key_LeftControl, Key_Semicolon);
    
    RegisterRepeatableEvent(mapping, AppCommandJumpNesting, Key_LeftControl, Key_J);
    RegisterRepeatableEvent(mapping, AppCommandIndent, Key_LeftControl, Key_Tab);
    RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_X);
    RegisterRepeatableEvent(mapping, AppCommandInsertSymbol, Key_LeftControl, Key_I);

    // Let the control commands bind its things
    ControlCommands_Initialize();
    ControlCommands_BindTrigger(mapping);
    
    appContext.freeTypeMapping = mapping;
}


void AppCommandJumpLeftArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_LEFT); }
void AppCommandJumpRightArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_RIGHT); }
void AppCommandJumpUpArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_UP); }
void AppCommandJumpDownArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_DOWN); }
void AppCommandLeftArrow(){ AppCommandFreeTypingArrows(DIRECTION_LEFT); }
void AppCommandRightArrow(){ AppCommandFreeTypingArrows(DIRECTION_RIGHT); }
void AppCommandUpArrow(){ AppCommandFreeTypingArrows(DIRECTION_UP); }
void AppCommandDownArrow(){ AppCommandFreeTypingArrows(DIRECTION_DOWN); }
