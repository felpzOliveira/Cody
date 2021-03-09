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
#include <file_buffer.h>
#include <autocomplete.h>
#include <types.h>

#define DIRECTION_LEFT  0
#define DIRECTION_UP    1
#define DIRECTION_DOWN  2
#define DIRECTION_RIGHT 3

#define MAX_VIEWS 16

typedef struct{
    BindingMap *freeTypeMapping;
    BindingMap *queryBarMapping;
    View *activeView;
    uint viewsCount;
    int activeId;
    
    char cwd[PATH_MAX];
    View views[MAX_VIEWS];
    FileBufferList fileBuffer;
}App;

static App appContext = { 
    .freeTypeMapping = nullptr, 
    .activeView = nullptr,
    .viewsCount = 0,
    .activeId = -1,
};

AppConfig appGlobalConfig;
int testHover = 1;

void AppInitializeFreeTypingBindings();
void AppInitializeQueryBarBindings();
BufferView *AppGetActiveBufferView();

void AppSetViewingGeometry(Geometry geometry, Float lineHeight){
    Float w = (Float)(geometry.upper.x - geometry.lower.x);
    Float h = (Float)(geometry.upper.y - geometry.lower.y);
    for(uint i = 0; i < appContext.viewsCount; i++){
        View *view = &appContext.views[i];
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
}

void AppEarlyInitialize(){
    View *view = nullptr;
#if 1
    view = &appContext.views[0];
    appContext.activeView = view;
    view->geometry.extensionX = vec2f(0.f, 0.5f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->geometry.lower = vec2ui();
    View_EarlyInitialize(view);
    View_SetActive(view, 0);
    
    view = &appContext.views[1];
    view->geometry.extensionX = vec2f(0.5f, 1.0f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->geometry.lower = vec2ui();
    View_EarlyInitialize(view);
    View_SetActive(view, 0);
    appContext.viewsCount = 2;
#else
    view = &appContext.views[0];
    appContext.activeView = view;
    view->geometry.extensionX = vec2f(0.f, 0.5f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->geometry.lower = vec2ui();
    View_EarlyInitialize(view, bView);
    View_SetActive(view, 0);
    appContext.viewsCount = 1;
#endif
    
    appContext.activeId = 0;
    view = &appContext.views[appContext.activeId];
    appContext.activeView = view;
    View_SetActive(appContext.activeView, 1);
    
    FileBufferList_Init(&appContext.fileBuffer);
    GetCurrentWorkingDirectory(appContext.cwd, PATH_MAX);
    
    //TODO: Configuration file
    appGlobalConfig.tabSpacing = 4;
    appGlobalConfig.useTabs = 0;
    appGlobalConfig.cStyle = CURSOR_RECT;
}

char *AppGetContextDirectory(){
    return &appContext.cwd[0];
}

int AppGetViewCount(){
    return appContext.viewsCount;
}

View *AppGetActiveView(){
    return appContext.activeView;
}

View *AppGetView(int i){
    return &appContext.views[i];
}

BufferView *AppGetBufferView(int i){
    return &appContext.views[i].bufferView;
}

BufferView *AppGetActiveBufferView(){
    return &appContext.activeView->bufferView;
}

void AppInsertLineBuffer(LineBuffer *lineBuffer){
    FileBufferList_Insert(&appContext.fileBuffer, lineBuffer);
}

FileBufferList *AppGetFileBufferList(){
    return &appContext.fileBuffer;
}

int AppIsFileLoaded(char *path, uint len){
    return FileBufferList_FindByPath(&appContext.fileBuffer, nullptr, path, len);
}

void AppSetBindingsForState(ViewState state){
    switch(state){
        case View_SelectableList:
        case View_QueryBar: KeyboardSetActiveMapping(appContext.queryBarMapping); break;
        case View_FreeTyping: KeyboardSetActiveMapping(appContext.freeTypeMapping); break;
        default:{
            AssertErr(0, "Not implemented binding");
        }
    }
}

void AppSetActiveView(int i){
    AssertA(i >= 0 && (uint)i < appContext.viewsCount, "Invalid view id");
    if(appContext.activeView){
        View_SetActive(appContext.activeView, 0);
    }
    appContext.activeView = &appContext.views[i];
    View_SetActive(appContext.activeView, 1);
    ViewState state = View_GetState(appContext.activeView);
    
    AppSetBindingsForState(state);
    appContext.activeId = i;
}

void AppUpdateViews(){
    for(uint i = 0; i < appContext.viewsCount; i++){
        BufferView *view = AppGetBufferView(i);
        BufferView_Synchronize(view);
    }
}

void AppCommandFreeTypingJumpToDirection(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
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
                cursor.y = Buffer_Utf8RawPositionToPosition(buffer, token->position + token->size);
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
            
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_UP:{ // Move Up
            cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_DOWN:{ // Move down
            cursor.x = Clamp(cursor.x+1, (uint)0, lineCount-1);
            
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
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
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        
        default: AssertA(0, "Invalid direction given");
    }
}

void RemountTokensBasedOn(BufferView *view, uint base, uint offset=0){
    LineBuffer *lineBuffer = view->lineBuffer;
    Tokenizer *tokenizer = view->tokenizer;
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, base, offset);
}

void AppOnModifiedToken(BufferView *view, Buffer *buffer, Token *modifiedToken){
    Tokenizer *tokenizer = view->tokenizer;
    SymbolTable *symTable = tokenizer->symbolTable;
    if(modifiedToken){
        if(Lex_IsUserToken(modifiedToken)){
            /*
* It could happen that this token has no inner value because it is
* broken statement like 'enum class X' where one is defined but the
* other never gets a value.
*/
            if(modifiedToken->reserved != nullptr){
                char *label = (char *)modifiedToken->reserved;
                SymbolTable_Remove(symTable, label, modifiedToken->size,
                                   modifiedToken->identifier);
                
                if(modifiedToken->reserved){
                    AllocatorFree(modifiedToken->reserved);
                    modifiedToken->reserved = nullptr;
                }
            }
        }
    }
}


void AppQueryBarSearchJumpToResult(QueryBar *bar, View *view){
    QueryBarCmdSearch *result = nullptr;
    BufferView *bView = View_GetBufferView(view);
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
    if(state == View_FreeTyping){
        BufferView *bufferView = View_GetBufferView(view);
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
        
        NullRet(buffer);
        
        if(cursor.y > 0){
            vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
            if(id.x != (int)cursor.x || id.y != OPERATION_REMOVE_CHAR){
                // user started to edit a different buffer
                UndoRedoUndoPushInsert(&bufferView->lineBuffer->undoRedo, buffer, cursor);
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                           vec2i((int)cursor.x, OPERATION_REMOVE_CHAR));
            }
            
            uint tid = Buffer_GetTokenAt(buffer, cursor.y-1);
            Token *token = &buffer->tokens[tid];
            AppOnModifiedToken(bufferView, buffer, token);
            
            Buffer_RemoveRange(buffer, cursor.y-1, cursor.y);
            RemountTokensBasedOn(bufferView, cursor.x);
            cursor.y -= 1;
            
        }else if(cursor.x > 0){
            int offset = buffer->count;
            
            if(buffer->tokenCount > 0){
                Token *token = &buffer->tokens[0];
                AppOnModifiedToken(bufferView, buffer, token);
            }
            
            Buffer *pBuffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            if(pBuffer->tokenCount > 0){
                Token *token = &pBuffer->tokens[pBuffer->tokenCount-1];
                AppOnModifiedToken(bufferView, pBuffer, token);
            }
            
            LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
            buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            RemountTokensBasedOn(bufferView, cursor.x-1);
            
            cursor.y = buffer->count - offset;
            cursor.x -= 1;
            UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
        }
        
        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
        bufferView->lineBuffer->is_dirty = 1;
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
        
        uint u8tp = Buffer_Utf8RawPositionToPosition(buffer, token->position);
        Buffer_RemoveRange(buffer, u8tp, cursor.y);
        cursor.y = u8tp;
        RemountTokensBasedOn(bufferView, cursor.x);
    }else if(cursor.x > 0){
        int offset = buffer->count;
        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
        buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
        RemountTokensBasedOn(bufferView, cursor.x-1);
        
        cursor.y = buffer->count - offset;
        cursor.x -= 1;
        UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
    }
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    
    BufferView_AdjustGhostCursorIfOut(bufferView);
    bufferView->lineBuffer->is_dirty = 1;
}

void AppCommandInsertTab(){
    BufferView *bufferView = AppGetActiveBufferView();
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
    bufferView->lineBuffer->is_dirty = 1;
}

void AppCommandRemoveTextBlock(BufferView *bufferView, vec2ui start, vec2ui end){
    Buffer *buffer = BufferView_GetBufferAt(bufferView, start.x);
    if(start.x == end.x){
        if(buffer->tokenCount > 0){
            uint si = Buffer_GetTokenAt(buffer, start.y);
            uint ei = Min(Buffer_GetTokenAt(buffer, end.y), buffer->tokenCount-1);
            for(uint i = si; i <= ei; i++){
                Token *token = &buffer->tokens[i];
                AppOnModifiedToken(bufferView, buffer, token);
            }
        }
        
        Buffer_RemoveRange(buffer, start.y, end.y);
    }else{
        uint rmov = 0;
        uint si = Buffer_GetTokenAt(buffer, start.y);
        for(uint i = si; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            AppOnModifiedToken(bufferView, buffer, token);
        }
        
        Buffer_RemoveRange(buffer, start.y, buffer->count);
        for(uint i = start.x + 1; i < end.x; i++){
            Buffer *b0 = BufferView_GetBufferAt(bufferView, start.x+1);
            for(uint j = 0; j < b0->tokenCount; j++){
                Token *token = &b0->tokens[j];
                AppOnModifiedToken(bufferView, b0, token);
            }
            
            LineBuffer_RemoveLineAt(bufferView->lineBuffer, start.x+1);
            rmov++;
        }
        
        // remove end now
        buffer = BufferView_GetBufferAt(bufferView, end.x - rmov);
        uint ei = Buffer_GetTokenAt(buffer, end.y);
        ei = Min(ei, buffer->tokenCount > 0 ? buffer->tokenCount-1 : 0);
        for(uint i = 0; i <= ei; i++){
            Token *token = &buffer->tokens[i];
            AppOnModifiedToken(bufferView, buffer, token);
        }
        
        Buffer_RemoveRange(buffer, 0, end.y);
        
        // merge start and end
        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, start.x);
        bufferView->lineBuffer->is_dirty = 1;
    }
}

vec2ui AppCommandNewLine(BufferView *bufferView, vec2ui at){
    char *lineHelper = nullptr;
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, at.x);
    Buffer *bufferp1 = BufferView_GetBufferAt(bufferView, at.x+1);
    uint len = 0;
    
    if(bufferp1){
        TokenizerStateContext *ctx = &bufferp1->stateContext;
        len = appGlobalConfig.tabSpacing * ctx->indentLevel;
    }else{
        uint s = AppComputeLineLastIndentLevel(buffer);
        len = appGlobalConfig.tabSpacing * s;
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
    
    RemountTokensBasedOn(bufferView, at.x, 1);
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
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    NullRet(buffer);
    
    UndoRedoUndoPushMerge(&bufferView->lineBuffer->undoRedo, buffer, cursor);
    
    cursor = AppCommandNewLine(bufferView, cursor);
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_AdjustGhostCursorIfOut(bufferView);
    bufferView->lineBuffer->is_dirty = 1;
    
}

//TODO
void AppCommandRedo(){
    
}

void AppCommandKillBuffer(){
    BufferView *bufferView = AppGetActiveBufferView();
    LineBuffer *lineBuffer = bufferView->lineBuffer;
    if(lineBuffer){
        Tokenizer *tokenizer = bufferView->tokenizer;
        SymbolTable *symTable = tokenizer->symbolTable;
        for(uint i = 0; i < lineBuffer->lineCount; i++){
            Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
            for(uint k = 0; k < buffer->tokenCount; k++){
                Token *token = &buffer->tokens[k];
                if(Lex_IsUserToken(token)){
                    if(token->reserved != nullptr){
                        char *label = (char *)token->reserved;
                        SymbolTable_Remove(symTable, label, token->size, token->identifier);
                    }
                    
                    if(token->reserved){
                        AllocatorFree(token->reserved);
                        token->reserved = nullptr;
                    }
                }
            }
        }
        
        for(uint i = 0; i < appContext.viewsCount; i++){
            BufferView *bView = AppGetBufferView(i);
            if(bView->lineBuffer == lineBuffer){
                BufferView_SwapBuffer(bView, nullptr);
            }
        }
        
        char *ptr = lineBuffer->filePath;
        uint pSize = lineBuffer->filePathSize;
        FileBufferList_Remove(&appContext.fileBuffer, ptr, pSize);
        LineBuffer_Free(lineBuffer);
        AllocatorFree(lineBuffer);
    }
}

void AppCommandUndo(){
    BufferView *bufferView = AppGetActiveBufferView();
    LineBuffer *lineBuffer = bufferView->lineBuffer;
    BufferChange *bChange = UndoRedoGetNextUndo(&lineBuffer->undoRedo);
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
                    LineBuffer_RemoveLineAt(bufferView->lineBuffer,
                                            bChange->bufferInfo.x+1);
                }
                
                context = &buffer->stateContext;
                fTrack = Max(fTrack, context->forwardTrack);
                bTrack = Max(bTrack, context->backTrack);
                
                for(uint i = 0; i < buffer->tokenCount; i++){
                    Token *token = &buffer->tokens[i];
                    AppOnModifiedToken(bufferView, buffer, token);
                }
                
                UndoRedoPopUndo(&lineBuffer->undoRedo);
                UndoSystemTakeBuffer(buffer);
                
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                uint base = cursor.x > bTrack ? cursor.x - bTrack : 0;
                RemountTokensBasedOn(bufferView, base, fTrack);
                // restore the active buffer and let stack grow again
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(-1,-1));
            } break;
            
            case CHANGE_BLOCK_REMOVE:{
                vec2ui start = bChange->bufferInfo;
                vec2ui end   = bChange->bufferInfoEnd;
                cursor = start;
                
                printf("Removing range [%u %u] x [%u %u]\n",
                       start.x, start.y, end.x, end.y); 
                
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
                                                        start.x, start.y, 
                                                        bufferView->tokenizer, &off);
                    
                    uint endx = start.x + n;
                    uint endy = off;
                    LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(-1, -1));
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
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    cursor.y = 0;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppCommandQueryBarSearch(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
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
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_REVERSE_SEARCH, view);
        View_ReturnToState(view, View_QueryBar);
        
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandEndLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = buffer->count;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppCommandSwapView(){
    if((uint)appContext.activeId + 1 < appContext.viewsCount){
        AppSetActiveView(appContext.activeId+1);
    }else{
        AppSetActiveView(0);
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
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView);
    
    vec2ui cursor = BufferView_GetCursorPosition(bView);
    Buffer *buffer = BufferView_GetBufferAt(bView, cursor.x);
    NullRet(buffer);
    
    uint tid = Buffer_GetTokenAt(buffer, cursor.y > 0 ? cursor.y - 1 : 0);
    if(buffer->tokenCount > 0){
        if(tid <= buffer->tokenCount-1){
            Token *token = &buffer->tokens[tid];
            char *ptr = &buffer->data[token->position];
            AutoComplete_Search(ptr, token->size);
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
        LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetStringX11(ptr);
        printf("Copied %s === \n", ptr);
    }
}

void AppCommandJumpNesting(){
    DoubleCursor *cursor = nullptr;
    BufferView *view = AppGetActiveBufferView();
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
    
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetStringX11(ptr);
        printf("Copied %s === \n", ptr);
        
        UndoRedoUndoPushInsertBlock(&view->lineBuffer->undoRedo, start, ptr, size);
        AppCommandRemoveTextBlock(view, start, end);
        
        RemountTokensBasedOn(view, start.x, 1);
        BufferView_CursorToPosition(view, start.x, start.y);
        BufferView_AdjustGhostCursorIfOut(view);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppCommandPaste(){
    uint size = 0;
    const char *p = ClipboardGetStringX11(&size);
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    
    if(size > 0 && p){
        Buffer *buffer = nullptr;
        Token *mToken = nullptr;
        uint off = 0;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        uint startBuffer = cursor.x;
        buffer = BufferView_GetBufferAt(view, cursor.x);
        
        // check first token
        if(cursor.y > 0){
            uint tid = Buffer_GetTokenAt(buffer, cursor.y-1);
            if(tid < buffer->tokenCount){
                mToken = &buffer->tokens[tid];
            }
        }else if(buffer->tokenCount > 0){
            mToken = &buffer->tokens[0];
        }
        
        AppOnModifiedToken(view, buffer, mToken);
        
        // check the next token as well
        if(buffer->tokenCount > 0){
            uint tid = Buffer_GetTokenAt(buffer, cursor.y);
            if(tid < buffer->tokenCount){
                mToken = &buffer->tokens[tid];
                AppOnModifiedToken(view, buffer, mToken);
            }
        }
        
        uint n = LineBuffer_InsertRawTextAt(view->lineBuffer, (char *) p, size, 
                                            cursor.x, cursor.y, view->tokenizer, &off);
        
        uint endx = cursor.x + n;
        uint endy = off;
        
        UndoRedoUndoPushRemoveBlock(&view->lineBuffer->undoRedo,
                                    cursor, vec2ui(endx, endy));
        LineBuffer_SetActiveBuffer(view->lineBuffer, vec2i(-1, -1));
        
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
            continue;
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
        
        //TODO: Maybe we can batch all lines as a CHANGE_BLOCK_INSERT for simpler undo.
        // Save undo - redo stuff as simple inserts
        UndoRedoUndoPushInsert(&view->lineBuffer->undoRedo, buffer, vec2ui(i, 0));
        
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
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        AppCommandIndentRegion(view, start, end);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppDefaultEntry(char *utf8Data, int utf8Size){
    if(utf8Size > 0){
        int off = 0;
        int cp = StringToCodepoint(utf8Data, utf8Size, &off);
        if(Font_SupportsCodepoint(cp)){
            View *view = AppGetActiveView();
            ViewState state = View_GetState(view);
            if(state == View_FreeTyping){
                BufferView *bufferView = AppGetActiveBufferView();
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
                
#if 0
                if(cursor.y > 0){
                    uint tid = Buffer_GetTokenAt(buffer, cursor.y-1);
                    if(tid < buffer->tokenCount){
                        token = &buffer->tokens[tid];
                    }
                }else if(buffer->tokenCount > 0){
                    token = &buffer->tokens[0];
                }
                
                AppOnModifiedToken(bufferView, buffer, token);
#endif
                Buffer_InsertStringAt(buffer, cursor.y, utf8Data, utf8Size);
                Buffer_Claim(buffer);
                RemountTokensBasedOn(bufferView, cursor.x);
                
                cursor.y += 1;
                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
                bufferView->lineBuffer->is_dirty = 1;
            }else if(View_IsQueryBarActive(view)){
                QueryBar *bar = View_GetQueryBar(view);
                if(QueryBar_AddEntry(bar, view, utf8Data, utf8Size) != 0){
                    AppQueryBarSearchJumpToResult(bar, view); // TODO: other choises
                }
            }
        }
    }
}

View *AppGetViewAt(int x, int y){
    View *rView = nullptr;
    int vCount = AppGetViewCount();
    vec2ui r(x, y);
    for(int i = 0; i < vCount; i++){
        View *view = AppGetView(i);
        if(Geometry_IsPointInside(&view->geometry, r)){
            rView = view;
            break;
        }
    }
    
    return rView;
}

vec2ui AppActivateViewAt(int x, int y){
    // Check which view is the mouse on and activate it
    int vCount = AppGetViewCount();
    vec2ui r(x, y);
    for(int i = 0; i < vCount; i++){
        View *view = AppGetView(i);
        if(Geometry_IsPointInside(&view->geometry, r)){
            // Activate the view and re-map position
            AppSetActiveView(i);
            r = r - view->geometry.lower;
            break;
        }
    }
    
    return r;
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

void AppCommandSwitchBuffer(){
    View *view = AppGetActiveView();
    int r = SwitchBufferCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppCommandOpenFile(){
    View *view = AppGetActiveView();
    auto fileOpen = [](View *view, FileEntry *entry) -> void{
        char targetPath[PATH_MAX];
        LineBuffer *lBuffer = nullptr;
        BufferView *bView = View_GetBufferView(view);
        if(entry){
            FileOpener *opener = View_GetFileOpener(view);
            uint l = snprintf(targetPath, PATH_MAX, "%s%s", opener->basePath, entry->path);
            if(entry->isLoaded == 0){
                uint fileSize = 0;
                char *content = GetFileContents(targetPath, &fileSize);
                
                lBuffer = AllocatorGetN(LineBuffer, 1);
                *lBuffer = LINE_BUFFER_INITIALIZER;
                
                //TODO: Can we center this call somewhere instead of manually
                //      needing to invoke a tokenizer state cleanup?
                Lex_TokenizerContextReset(bView->tokenizer);
                
                if(fileSize == 0){
                    AllocatorFree(content);
                    LineBuffer_InitEmpty(lBuffer);
                }else{
                    LineBuffer_Init(lBuffer, bView->tokenizer, content, fileSize);
                }
                
                LineBuffer_SetStoragePath(lBuffer, targetPath, l);
                AllocatorFree(content);
                
                BufferView_SwapBuffer(bView, lBuffer);
                AppInsertLineBuffer(lBuffer);
            }else{
                int f = FileBufferList_FindByPath(&appContext.fileBuffer, &lBuffer,
                                                  targetPath, l);
                if(f == 0 || lBuffer == nullptr){
                    printf("Did not find buffer %s\n", entry->path);
                }else{
                    printf("Swapped to %s\n", entry->path);
                    BufferView_SwapBuffer(bView, lBuffer);
                }
            }
        }else{ // is file creation (or dir?)
            //TODO: Directories?
            char *content = nullptr;
            uint len = 0;
            QueryBar *querybar = View_GetQueryBar(view);
            QueryBar_GetWrittenContent(querybar, &content, &len);
            if(len > 0){
                printf("Creating file %s\n", content);
                lBuffer = AllocatorGetN(LineBuffer, 1);
                *lBuffer = LINE_BUFFER_INITIALIZER;
                LineBuffer_InitEmpty(lBuffer);
                
                LineBuffer_SetStoragePath(lBuffer, content, len);
                
                BufferView_SwapBuffer(bView, lBuffer);
                AppInsertLineBuffer(lBuffer);
            }
        }
    };
    
    int r = FileOpenerCommandStart(view, appContext.cwd, 
                                   strlen(appContext.cwd), fileOpen);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
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
    __memory_freeze();
    printf(" === ==== === FREEZED STATE\n");
}

void AppMemoryDebugCmp(){
    __memory_compare_state();
    getchar();
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
    
    RegisterRepeatableEvent(mapping, AppCommandSaveBufferView, Key_LeftAlt, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarSearch, Key_LeftControl, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftControl, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftAlt, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarRevSearch, Key_LeftControl, Key_R);
    RegisterRepeatableEvent(mapping, AppCommandSwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, AppCommandInsertTab, Key_Tab);
    
    RegisterRepeatableEvent(mapping, AppCommandPaste, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_C);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_Q);
    
    RegisterRepeatableEvent(mapping, AppCommandUndo, Key_LeftControl, Key_Z);
    //RegisterRepeatableEvent(mapping, AppCommandRedo, Key_LeftControl, Key_R);
    
    RegisterRepeatableEvent(mapping, AppCommandJumpNesting, Key_LeftControl, Key_B);
    RegisterRepeatableEvent(mapping, AppCommandIndent, Key_LeftControl, Key_Tab);
    RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_X);
    
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
