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
    BufferView *view = &appContext.views[0];
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
    
    appContext.activeBufferView = view;
    appContext.activeBufferView->isActive = 1;
    appContext.activeId = 1;
    
    appGlobalConfig.tabSpacing = 4;
    appGlobalConfig.useTabs = 1;
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
        
        Buffer_RemoveRange(buffer, cursor.y-1, cursor.y);
        RemountTokensBasedOn(bufferView, cursor.x);
        cursor.y -= 1;
        
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
        Buffer_RemoveRange(buffer, start.y, end.y);
    }else{
        uint rmov = 0;
        Buffer_RemoveRange(buffer, start.y, buffer->count);
        for(uint i = start.x + 1; i < end.x; i++){
            LineBuffer_RemoveLineAt(bufferView->lineBuffer, start.x+1);
            rmov++;
        }
        
        // remove end now
        buffer = BufferView_GetBufferAt(bufferView, end.x - rmov);
        Buffer_RemoveRange(buffer, 0, end.y);
        
        // merge start and end
        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, start.x);
    }
}

vec2ui AppCommandNewLine(BufferView *bufferView, vec2ui at){
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, at.x);
    
    int toNextLine = Max((int)buffer->count - (int)at.y, 0);
    char *dataptr = nullptr;
    if(toNextLine > 0){
        uint p = Buffer_Utf8PositionToRawPosition(buffer, (uint)at.y, nullptr);
        dataptr = &buffer->data[p];
        toNextLine = buffer->taken - p;
    }
    
    LineBuffer_InsertLineAt(lineBuffer, at.x+1, dataptr, toNextLine, 0);
    if(toNextLine > 0){
        buffer = BufferView_GetBufferAt(bufferView, at.x);
        Buffer_RemoveRange(buffer, at.y, buffer->count);
    }
    
    RemountTokensBasedOn(bufferView, at.x, 1);
    buffer = BufferView_GetBufferAt(bufferView, at.x);
    
    at.x += 1;
    at.y = 0;
    return at;
}

void AppCommandNewLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    uint at = cursor.y;
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
        switch(bChange->change){
            case CHANGE_MERGE:{
                LineBuffer_MergeConsecutiveLines(lineBuffer, bChange->bufferInfo.x);
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
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
                Buffer *buffer = LineBuffer_ReplaceBufferAt(bufferView->lineBuffer, b,
                                                            bChange->bufferInfo.x);
                printf("Giving buffer %p\n", b);
                
                UndoRedoPopUndo(&lineBuffer->undoRedo);
                UndoSystemTakeBuffer(buffer);
                
                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                RemountTokensBasedOn(bufferView, cursor.x);
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
    
    if(cursor->nestValid){
        //TODO: Create toogle
        if(view->activeNestPoint == -1){
            view->activeNestPoint = 0;
        }else{
            view->activeNestPoint = 1 - view->activeNestPoint;
        }
        
        vec2ui p = view->activeNestPoint == 0 ? cursor->nestStart : cursor->nestEnd;
        Buffer *buffer = BufferView_GetBufferAt(view, p.x);
        BufferView_CursorToPosition(view, p.x, buffer->tokens[p.y].position);
    }
}

void AppCommandPaste(){
    uint size = 0;
    const char *p = ClipboardGetStringX11(&size);
    if(size > 0 && p){
        Buffer *buffer = nullptr;
        uint off = 0;
        BufferView *view = AppGetActiveBufferView();
        vec2ui cursor = BufferView_GetCursorPosition(view);
        uint startBuffer = cursor.x;
        uint n = LineBuffer_InsertRawTextAt(view->lineBuffer, (char *) p, size, 
                                            cursor.x, cursor.y, view->tokenizer, &off);
        
        uint endx = cursor.x + n;
        endx = endx > 1 ? endx - 1 : endx;
        uint endy = off;
        
        UndoRedoUndoPushRemoveBlock(&view->lineBuffer->undoRedo,
                                    cursor, vec2ui(endx, endy));
        LineBuffer_SetActiveBuffer(view->lineBuffer, vec2i(-1, -1));
        
        RemountTokensBasedOn(view, startBuffer, n);
        
        cursor.x = endx;
        cursor.y = endy;
        BufferView_CursorToPosition(view, cursor.x, cursor.y);
    }
}

void AppDefaultEntry(char *utf8Data, int utf8Size){
    if(utf8Size > 0){
        int off = 0;
        int cp = StringToCodepoint(utf8Data, utf8Size, &off);
        if(Font_SupportsCodepoint(cp)){
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
    
    appContext.freeTypeMapping = mapping;
    KeyboardSetActiveMapping(appContext.freeTypeMapping);
}