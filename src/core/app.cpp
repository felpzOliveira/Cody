#include <app.h>
#include <keyboard.h>
#include <lex.h>
#include <utilities.h>
#include <font.h>
#include <bufferview.h>
#include <x11_display.h>
#include <undo.h>

#define DIRECTION_LEFT  0
#define DIRECTION_UP    1
#define DIRECTION_DOWN  2
#define DIRECTION_RIGHT 3

#define MAX_BUFFERVIEWS 16

typedef struct{
    BindingMap *freeTypeMapping;
    BufferView *activeBufferView;
    int viewsCount;
    int activeId;
    
    BufferView views[MAX_BUFFERVIEWS];
}App;

static App appContext = { 
    .freeTypeMapping = nullptr, 
    .activeBufferView = nullptr,
    .viewsCount = 0,
    .activeId = -1,
};

AppConfig appGlobalConfig;

void AppSetViewingGeometry(Geometry geometry, Float lineHeight){
    Float w = (Float)(geometry.upper.x - geometry.lower.x);
    Float h = (Float)(geometry.upper.y - geometry.lower.y);
    for(int i = 0; i < appContext.viewsCount; i++){
        BufferView *view = &appContext.views[i];
        uint x0 = (uint)(w * view->geometry.extensionX.x);
        uint x1 = (uint)(w * view->geometry.extensionX.y);
        uint y0 = (uint)(h * view->geometry.extensionY.x);
        uint y1 = (uint)(h * view->geometry.extensionY.y);
        
        Geometry targetGeo;
        targetGeo.lower = vec2ui(x0, y0);
        targetGeo.upper = vec2ui(x1, y1);
        targetGeo.extensionX = view->geometry.extensionX;
        targetGeo.extensionY = view->geometry.extensionY;
        BufferView_SetGeometry(view, targetGeo, lineHeight);
    }
}

void AppEarlyInitialize(){
    BufferView *view = nullptr;
#if 1
    view = &appContext.views[0];
    appContext.activeBufferView = view;
    view->geometry.extensionX = vec2f(0.f, 0.5f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->geometry.lower = vec2ui();
    view->isActive = 0;
    
    view = &appContext.views[1];
    view->geometry.extensionX = vec2f(0.5f, 1.0f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->isActive = 0;
    appContext.viewsCount = 2;
#else
    view = &appContext.views[0];
    appContext.activeBufferView = view;
    view->geometry.extensionX = vec2f(0.f, 0.5f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    view->geometry.lower = vec2ui();
    view->isActive = 0;
    appContext.viewsCount = 1;
#endif
    
    view = &appContext.views[0];
    appContext.activeBufferView = view;
    appContext.activeBufferView->isActive = 1;
    appContext.activeId = 0;
    
    appGlobalConfig.tabSpacing = 4;
    appGlobalConfig.useTabs = 0;
}

int AppGetBufferViewCount(){
    return appContext.viewsCount;
}

BufferView *AppGetBufferView(int i){
    return &appContext.views[i];
}

BufferView *AppGetActiveBufferView(){
    return appContext.activeBufferView;
}

void AppSetActiveBufferView(int i){
    AssertA(i >= 0 && i < appContext.viewsCount, "Invalid view id");
    if(appContext.activeBufferView){
        appContext.activeBufferView->isActive = 0;
    }
    appContext.activeBufferView = &appContext.views[i];
    appContext.activeBufferView->isActive = 1;
    appContext.activeId = i;
}

void AppUpdateViews(){
    BufferView *view = AppGetActiveBufferView();
    for(uint i = 0; i < appContext.viewsCount; i++){
        BufferView *other = &appContext.views[i];
        if(i != appContext.activeId && view->lineBuffer == other->lineBuffer){
            BufferView_SynchronizeWith(other, view);
        }
    }
}


void AppCommandFreeTypingJumpToDirection(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui visible = BufferView_GetViewRange(bufferView);
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

void AppCommandJumpLeftArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_LEFT); }
void AppCommandJumpRightArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_RIGHT); }
void AppCommandJumpUpArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_UP); }
void AppCommandJumpDownArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_DOWN); }

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
            cursor.y = Clamp(cursor.y, 0, buffer->count);
            
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_UP:{ // Move Up
            cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, 0, buffer->count);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_DOWN:{ // Move down
            cursor.x = Clamp(cursor.x+1, 0, lineCount-1);
            
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, 0, buffer->count);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        
        case DIRECTION_RIGHT:{ // Move right
            Buffer *buffer = LineBuffer_GetBufferAt(bufferView->lineBuffer, cursor.x);
            if(cursor.y >= buffer->count){ // move down
                cursor.x = Clamp(cursor.x+1, 0, lineCount-1);
            }else{
                cursor.y += 1;
            }
            
            buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, 0, buffer->count);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        
        default: AssertA(0, "Invalid direction given");
    }
}

void AppCommandLeftArrow(){ AppCommandFreeTypingArrows(DIRECTION_LEFT); }
void AppCommandRightArrow(){ AppCommandFreeTypingArrows(DIRECTION_RIGHT); }
void AppCommandUpArrow(){ AppCommandFreeTypingArrows(DIRECTION_UP); }
void AppCommandDownArrow(){ AppCommandFreeTypingArrows(DIRECTION_DOWN); }

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
                AllocatorFree(modifiedToken->reserved);
                modifiedToken->reserved = nullptr;
            }
        }
    }
}

void AppCommandRemoveOne(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
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
            uint ei = Max(Buffer_GetTokenAt(buffer, end.y), buffer->tokenCount-1);
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
    TokenizerStateContext *ctx = &bufferp1->stateContext;
    
    uint len = appGlobalConfig.tabSpacing * ctx->indentLevel;
    
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
    
    at.x += 1;
    at.y = Buffer_Utf8RawPositionToPosition(bufferp1, len);
    return at;
}

void AppCommandNewLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    uint at = cursor.y;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    cursor = AppCommandNewLine(bufferView, cursor);
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_AdjustGhostCursorIfOut(bufferView);
    bufferView->lineBuffer->is_dirty = 1;
    
    UndoRedoUndoPushMerge(&bufferView->lineBuffer->undoRedo, vec2ui(cursor.x-1, at));
}

void AppCommandRedo(){
    BufferView *bufferView = AppGetActiveBufferView();
    LineBuffer *lineBuffer = bufferView->lineBuffer;
    BufferChange *bChange = UndoRedoGetNextRedo(&lineBuffer->undoRedo);
    if(bChange){
        //TODO: types of commands
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        switch(bChange->change){
            case CHANGE_MERGE:{
                //NOTE: This function already pushes a operation into undo
                LineBuffer_MergeConsecutiveLines(lineBuffer, bChange->bufferInfo.x);
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                
                RemountTokensBasedOn(bufferView, cursor.x);
                Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
                UndoRedoUndoPushMerge(&lineBuffer->undoRedo,
                                      vec2ui(cursor.x, buffer->count));
            } break;
            case CHANGE_NEWLINE:{
                cursor = AppCommandNewLine(bufferView, bChange->bufferInfo);
                RemountTokensBasedOn(bufferView, cursor.x);
                UndoRedoUndoPushMerge(&lineBuffer->undoRedo, vec2ui(cursor.x-1,
                                                                    bChange->bufferInfo.y));
            } break;
            
            default:{ printf("Unknow undo command\n"); }
        }
        
        UndoRedoPopRedo(&lineBuffer->undoRedo);
        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
    }
}

void AppCommandUndo(){
    BufferView *bufferView = AppGetActiveBufferView();
    LineBuffer *lineBuffer = bufferView->lineBuffer;
    BufferChange *bChange = UndoRedoGetNextUndo(&lineBuffer->undoRedo);
    if(bChange){
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        //TODO: Redo
        printf("[APP] Undo for %s\n", UndoRedo_GetStringID(bChange->change));
        switch(bChange->change){
            case CHANGE_MERGE:{
                LineBuffer_MergeConsecutiveLines(lineBuffer, bChange->bufferInfo.x);
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
                Buffer_RemoveRange(buffer, cursor.y, buffer->count);
                
                RemountTokensBasedOn(bufferView, cursor.x);
                UndoRedoPopUndo(&lineBuffer->undoRedo);
                //UndoRedoRedoPushNewLine(&lineBuffer->undoRedo, bChange->bufferInfo);
            } break;
            
            case CHANGE_NEWLINE:{
                cursor = AppCommandNewLine(bufferView, bChange->bufferInfo);
                RemountTokensBasedOn(bufferView, cursor.x);
                UndoRedoPopUndo(&lineBuffer->undoRedo);
            } break;
            
            case CHANGE_REMOVE:
            case CHANGE_INSERT:{
                Buffer *b = bChange->buffer;
                TokenizerStateContext *context = &b->stateContext;
                
                uint fTrack = context->forwardTrack;
                uint bTrack = context->backTrack;
                
                Buffer *buffer = LineBuffer_ReplaceBufferAt(bufferView->lineBuffer, b,
                                                            bChange->bufferInfo.x);
                
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


void AppCommandEndLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = buffer->count;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppCommandSwapView(){
    if(appContext.activeId + 1 < appContext.viewsCount){
        AppSetActiveBufferView(appContext.activeId+1);
    }else{
        AppSetActiveBufferView(0);
    }
}

void AppCommandSetGhostCursor(){
    BufferView *bufferView = AppGetActiveBufferView();
    BufferView_GhostCursorFollow(bufferView);
}

void AppCommandSwapLineNbs(){
    BufferView *bufferView = AppGetActiveBufferView();
    BufferView_ToogleLineNumbers(bufferView);
}

void AppCommandLineQuicklyDisplay(){
    BufferView *bufferView = AppGetActiveBufferView();
    BufferView_StartNumbersShowTransition(bufferView, 3.0);
}

void AppCommandSaveBufferView(){
    BufferView *bufferView = AppGetActiveBufferView();
    LineBuffer_SaveToStorage(bufferView->lineBuffer);
    bufferView->lineBuffer->is_dirty = 0;
}

void AppCommandCopy(){
    vec2ui start, end;
    uint size = 0;
    char *ptr = nullptr;
    BufferView *view = AppGetActiveBufferView();
    
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
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
    
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetStringX11(ptr);
        printf("Copied %s === \n", ptr);
        
        UndoRedoUndoPushInsertBlock(&view->lineBuffer->undoRedo, start, ptr, size);
        AppCommandRemoveTextBlock(view, start, end);
        
        RemountTokensBasedOn(view, start.x, 1);
        BufferView_CursorToPosition(view, start.x, start.y);
        view->lineBuffer->is_dirty = 1;
    }
}

void AppCommandPaste(){
    uint size = 0;
    const char *p = ClipboardGetStringX11(&size);
    if(size > 0 && p){
        Buffer *buffer = nullptr;
        Token *mToken = nullptr;
        uint off = 0;
        BufferView *view = AppGetActiveBufferView();
        vec2ui cursor = BufferView_GetCursorPosition(view);
        uint startBuffer = cursor.x;
        buffer = BufferView_GetBufferAt(view, cursor.x);
        
        // check first token
        if(cursor.y > 0){
            uint tid = Buffer_GetTokenAt(buffer, cursor.y-1);
            mToken = &buffer->tokens[tid];
        }else if(buffer->tokenCount > 0){
            mToken = &buffer->tokens[0];
        }
        
        AppOnModifiedToken(view, buffer, mToken);
        
        // check the next token as well
        if(buffer->tokenCount > 0){
            uint tid = Buffer_GetTokenAt(buffer, cursor.y);
            mToken = &buffer->tokens[tid];
            AppOnModifiedToken(view, buffer, mToken);
        }
        
        uint n = LineBuffer_InsertRawTextAt(view->lineBuffer, (char *) p, size, 
                                            cursor.x, cursor.y, view->tokenizer, &off);
        
        uint endx = cursor.x + n;
        uint endy = off;
        
        UndoRedoUndoPushRemoveBlock(&view->lineBuffer->undoRedo,
                                    cursor, vec2ui(endx, endy));
        LineBuffer_SetActiveBuffer(view->lineBuffer, vec2i(-1, -1));
        
        RemountTokensBasedOn(view, startBuffer, n);
        
        cursor.x = endx;
        cursor.y = endy;
        BufferView_CursorToPosition(view, cursor.x, cursor.y);
        view->lineBuffer->is_dirty = 1;
    }
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
    vec2ui cursor = BufferView_GetCursorPosition(view);
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
            maxSize = len + llen;
            lineHelper = AllocatorExpand(char, lineHelper, maxSize);
        }
        
        // insert spacing only when there is actually a token
        if(len > 0 && foundNonSpace)
            Memset(lineHelper, indentChar, len * sizeof(char));
        // insert rest of the line
        if(llen > 0)
            Memcpy(&lineHelper[len], &buffer->data[targetP], llen * sizeof(char));
        
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
            Token *token = nullptr;
            BufferView *bufferView = AppGetActiveBufferView();
            vec2ui cursor = BufferView_GetCursorPosition(bufferView);
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            
            vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
            if(id.x != (int)cursor.x || id.y != OPERATION_INSERT_CHAR){
                // user started to edit a different buffer
                UndoRedoUndoPushRemove(&bufferView->lineBuffer->undoRedo, buffer, cursor);
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                           vec2i((int)cursor.x, OPERATION_INSERT_CHAR));
            }
            
            if(cursor.y > 0){
                uint tid = Buffer_GetTokenAt(buffer, cursor.y-1);
                token = &buffer->tokens[tid];
            }else if(buffer->tokenCount > 0){
                token = &buffer->tokens[0];
            }
            
            AppOnModifiedToken(bufferView, buffer, token);
            Buffer_InsertStringAt(buffer, cursor.y, utf8Data, utf8Size);
            
            RemountTokensBasedOn(bufferView, cursor.x);
            
            cursor.y += 1;
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
            bufferView->lineBuffer->is_dirty = 1;
        }
    }
}

vec2ui AppActivateBufferViewAt(int x, int y){
    // Check which view is the mouse on and activate it
    int vCount = AppGetBufferViewCount();
    vec2ui r(x, y);
    for(int i = 0; i < vCount; i++){
        BufferView *view = AppGetBufferView(i);
        if(Geometry_IsPointInside(&view->geometry, r)){
            // Activate the view and re-map position
            AppSetActiveBufferView(i);
            r = r - view->geometry.lower;
            break;
        }
    }
    
    return r;
}

void AppInitialize(){
    BindingMap *mapping = nullptr;
    KeyboardInitMappings();
    
    mapping = KeyboardCreateMapping();
    
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry);
    
    //TODO: Create basic mappings
    RegisterRepeatableEvent(mapping, AppCommandLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppCommandRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandUpArrow, Key_Up);
    RegisterRepeatableEvent(mapping, AppCommandDownArrow, Key_Down);
    
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftControl);
    
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftAlt);
    
    RegisterRepeatableEvent(mapping, AppCommandRemoveOne, Key_Backspace);
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
    
    RegisterRepeatableEvent(mapping, AppCommandSwapLineNbs, Key_LeftControl, Key_N);
    RegisterRepeatableEvent(mapping, AppCommandLineQuicklyDisplay, Key_LeftAlt, Key_N);
    
    RegisterRepeatableEvent(mapping, AppCommandSaveBufferView, Key_LeftAlt, Key_S);
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
    KeyboardSetActiveMapping(appContext.freeTypeMapping);
}