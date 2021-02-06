#include <app.h>
#include <keyboard.h>
#include <lex.h>
#include <utilities.h>
#include <font.h>

#define DIRECTION_LEFT  0
#define DIRECTION_UP    1
#define DIRECTION_DOWN  2
#define DIRECTION_RIGHT 3

#define MAX_BUFFERVIEWS 16

typedef struct{
    BindingMap *freeTypeMapping;
    BufferView *activeBufferView;
    int viewsCount;
    
    BufferView views[MAX_BUFFERVIEWS];
}App;

static App appContext = { 
    .freeTypeMapping = nullptr, 
    .activeBufferView = nullptr,
    .viewsCount = 0,
};

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
    
    view = &appContext.views[1];
    view->geometry.extensionX = vec2f(0.5f, 1.0f);
    view->geometry.extensionY = vec2f(0.f, 1.0f);
    appContext.viewsCount = 2;
    
    appContext.activeBufferView = view;
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

void AppBindingFreeTypingJumpToDirection(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    
    Token *token = nullptr;
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    switch(direction){
        case DIRECTION_LEFT:{ // Move left
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int tid = BufferView_LocatePreviousCursorToken(bufferView, buffer, &token);
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

void AppBindingJumpLeftArrow(){ AppBindingFreeTypingJumpToDirection(DIRECTION_LEFT); }
void AppBindingJumpRightArrow(){ AppBindingFreeTypingJumpToDirection(DIRECTION_RIGHT); }
void AppBindingJumpUpArrow(){ AppBindingFreeTypingJumpToDirection(DIRECTION_UP); }
void AppBindingJumpDownArrow(){ AppBindingFreeTypingJumpToDirection(DIRECTION_DOWN); }

void AppBindingFreeTypingArrows(int direction){
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

void AppBindingLeftArrow(){ AppBindingFreeTypingArrows(DIRECTION_LEFT); }
void AppBindingRightArrow(){ AppBindingFreeTypingArrows(DIRECTION_RIGHT); }
void AppBindingUpArrow(){ AppBindingFreeTypingArrows(DIRECTION_UP); }
void AppBindingDownArrow(){ AppBindingFreeTypingArrows(DIRECTION_DOWN); }

void RemountTokensBasedOn(BufferView *view, uint base, uint offset=0){
    LineBuffer *lineBuffer = view->lineBuffer;
    Tokenizer *tokenizer = view->tokenizer;
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, base, offset);
}

void AppBindingRemoveOne(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    if(cursor.y > 0){
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
    }
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppBindingRemovePreviousToken(){
    Token *token = nullptr;
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    int tid = BufferView_LocatePreviousCursorToken(bufferView, buffer, &token);
    if(tid >= 0){
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
    }
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}


void AppBindingNewLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    int toNextLine = Max((int)buffer->count - (int)cursor.y, 0);
    char *dataptr = nullptr;
    if(toNextLine > 0){
        dataptr = &buffer->data[cursor.y];
    }
    
    LineBuffer_InsertLineAt(lineBuffer, cursor.x+1, dataptr, toNextLine);
    if(toNextLine > 0){
        buffer = BufferView_GetBufferAt(bufferView, cursor.x);
        Buffer_RemoveRange(buffer, cursor.y, buffer->count);
    }
    
    RemountTokensBasedOn(bufferView, cursor.x, 1);
    buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    
    cursor.x += 1;
    cursor.y = 0;
    
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppBindingHomeLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    cursor.y = 0;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}


void AppBindingEndLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = buffer->count;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppDefaultEntry(char *utf8Data, int utf8Size){
    if(utf8Size > 0){
        int off = 0;
        int cp = StringToCodepoint(utf8Data, utf8Size, &off);
        if(Font_SupportsCodepoint(cp)){
            BufferView *bufferView = AppGetActiveBufferView();
            vec2ui cursor = BufferView_GetCursorPosition(bufferView);
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            Buffer_InsertStringAt(buffer, cursor.y, utf8Data, utf8Size);
            
            RemountTokensBasedOn(bufferView, cursor.x);
            
            cursor.y += 1;
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        }
    }
}

void AppInitialize(){
    BindingMap *mapping = nullptr;
    KeyboardInitMappings();
    
    mapping = KeyboardCreateMapping();
    
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry);
    
    //TODO: Create basic mappings
    RegisterRepeatableEvent(mapping, AppBindingLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppBindingRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppBindingUpArrow, Key_Up);
    RegisterRepeatableEvent(mapping, AppBindingDownArrow, Key_Down);
    
    RegisterRepeatableEvent(mapping, AppBindingJumpLeftArrow, Key_Left, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppBindingJumpRightArrow, Key_Right, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppBindingJumpUpArrow, Key_Up, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppBindingJumpDownArrow, Key_Down, Key_LeftControl);
    
    RegisterRepeatableEvent(mapping, AppBindingJumpLeftArrow, Key_Left, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppBindingJumpRightArrow, Key_Right, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppBindingJumpUpArrow, Key_Up, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppBindingJumpDownArrow, Key_Down, Key_LeftAlt);
    
    RegisterRepeatableEvent(mapping, AppBindingRemoveOne, Key_Backspace);
    RegisterRepeatableEvent(mapping, AppBindingRemovePreviousToken,
                            Key_Backspace, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppBindingRemovePreviousToken,
                            Key_Backspace, Key_LeftAlt);
    
    RegisterRepeatableEvent(mapping, AppBindingNewLine, Key_Enter);
    
    RegisterRepeatableEvent(mapping, AppBindingHomeLine, Key_Home);
    RegisterRepeatableEvent(mapping, AppBindingEndLine, Key_End);
    
    appContext.freeTypeMapping = mapping;
    KeyboardSetActiveMapping(appContext.freeTypeMapping);
}