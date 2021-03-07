#include <query_bar.h>
#include <view.h>
#include <bufferview.h>

static int QueryBar_AcceptInput(QueryBar *queryBar, char *input, uint len){
    QueryBarInputFilter *filter = &queryBar->filter;
    if(filter->digitOnly){
        return StringIsDigits(input, len);
    }
    
    return 1;
}

static int QueryBar_AreCommandsContinuous(QueryBar *queryBar, QueryBarCommand cmd){
    /*
* TODO: Check if the new command can borrow the already presented content of
* the query bar, i.e.: search - rever-search.
*/
    QueryBarCommand id = queryBar->cmd;
    return ((id == QUERY_BAR_CMD_REVERSE_SEARCH && cmd == QUERY_BAR_CMD_SEARCH) ||
            (id == QUERY_BAR_CMD_SEARCH && cmd == QUERY_BAR_CMD_REVERSE_SEARCH));
}

/* Sets tittle and ids */
static void QueryBar_StartCommand(QueryBar *queryBar, QueryBarCommand cmd,
                                  char *name=nullptr, uint titleLen=0)
{
    char title[32];
    uint len = 0;
    int restoreCursor = 1;
    uint lastp = queryBar->cursor.textPosition.y - queryBar->writePosU8;
    queryBar->searchCmd.valid = 0;
    queryBar->filter.digitOnly = 0;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    switch(cmd){
        case QUERY_BAR_CMD_SEARCH:{
            len = snprintf(title, sizeof(title), "Search: ");
        } break;
        case QUERY_BAR_CMD_REVERSE_SEARCH:{
            len = snprintf(title, sizeof(title), "Rev-Search: ");
        } break;
        case QUERY_BAR_CMD_JUMP_TO_LINE:{
            len = snprintf(title, sizeof(title), "Goto Line: ");
            queryBar->filter.digitOnly = 1;
        } break;
        case QUERY_BAR_CMD_CUSTOM:{
            if(title && titleLen > 0){
                len = snprintf(title, sizeof(title), "%s: ", name);
            }
        } break;
        default:{
            AssertErr(0, "Command not implemented");
        }
    }
    
    if(!QueryBar_AreCommandsContinuous(queryBar, cmd)){
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->buffer.count);
        lastp = 0;
    }else{
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->writePosU8);
    }
    
    queryBar->writePos = Buffer_InsertStringAt(&queryBar->buffer, 0, title, len, 0);
    queryBar->writePosU8 = Buffer_Utf8RawPositionToPosition(&queryBar->buffer,
                                                            queryBar->writePos);
    queryBar->cursor.textPosition.y = queryBar->writePosU8 + lastp;
    queryBar->cursor.textPosition.x = 0;
    queryBar->cmd = cmd;
}

//TODO: Check if it is a dynamic command, i.e: execute on type.
static int  QueryBar_DynamicUpdate(QueryBar *queryBar, View *view){
    int r = 0;
    switch(queryBar->cmd){
        case QUERY_BAR_CMD_SEARCH:
        case QUERY_BAR_CMD_REVERSE_SEARCH:{
            BufferView *bView = View_GetBufferView(view);
            r = QueryBarCommandSearch(queryBar, bView->lineBuffer,
                                      &bView->sController.cursor);
        } break;
        case QUERY_BAR_CMD_CUSTOM:{
            if(queryBar->entryCallback){
                r = queryBar->entryCallback(queryBar, view);
            }
        } break;
        default:{}
    }
    
    return r;
}

void QueryBar_Free(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    Buffer_Free(&queryBar->buffer);
    queryBar->isActive = 0;
    queryBar->writePos = 0;
    queryBar->writePosU8 = 0;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    queryBar->cmd = QUERY_BAR_CMD_NONE;
}

/*
* Sets the query bar to null values.
*/
void QueryBar_Initialize(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->buffer = BUFFER_INITIALIZER;
    queryBar->isActive = 0;
    queryBar->writePos = 0;
    queryBar->writePosU8 = 0;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    queryBar->cmd = QUERY_BAR_CMD_NONE;
    Buffer_Init(&queryBar->buffer, DefaultAllocatorSize);
}

/* Moves cursor left */
void QueryBar_MoveLeft(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->cursor.textPosition.y = Max(queryBar->cursor.textPosition.y - 1,
                                          queryBar->writePosU8);
}

/* Moves cursor right */
void QueryBar_MoveRight(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->cursor.textPosition.y = Min(queryBar->cursor.textPosition.y + 1,
                                          queryBar->buffer.count);
}

/* Sets query bar geometry */
void QueryBar_SetGeometry(QueryBar *queryBar, Geometry *geometry, Float lineHeight){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->geometry.lower = geometry->lower;
    queryBar->geometry.upper = vec2ui(geometry->upper.x, geometry->lower.y + lineHeight);
    queryBar->geometry.extensionX = geometry->extensionX;
    queryBar->geometry.extensionY = geometry->extensionY;
}

/*
* Makes the query bar prepare for a new command. Changes its state and sets/resets
* title and contents.
*/
void QueryBar_Activate(QueryBar *queryBar, QueryBarCommand cmd, View *view){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    BufferView *bView = View_GetBufferView(view);
    queryBar->cursorBackup = BufferView_GetCursorPosition(bView);
    QueryBar_StartCommand(queryBar, cmd);
}

void QueryBar_ActivateCustom(QueryBar *queryBar, char *title, uint titlelen, 
                             OnQueryBarEntry entry, OnQueryBarCancel cancel,
                             OnQueryBarCommit commit, QueryBarInputFilter *filter)
{
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_CUSTOM, title, titlelen);
    
    queryBar->entryCallback  = entry;
    queryBar->cancelCallback = cancel;
    queryBar->commitCallback = commit;
    if(filter) queryBar->filter = *filter;
}

void QueryBar_GetWrittenContent(QueryBar *queryBar, char **ptr, uint *len){
    AssertA(queryBar != nullptr && ptr != nullptr && len != nullptr,
            "Invalid QueryBar pointer");
    
    *ptr = &queryBar->buffer.data[queryBar->writePos];
    *len = queryBar->buffer.taken - queryBar->writePos;
}

void QueryBar_GetTitle(QueryBar *queryBar, char **ptr, uint *len){
    AssertA(queryBar != nullptr && ptr != nullptr && len != nullptr,
            "Invalid QueryBar pointer");
    *ptr = queryBar->buffer.data;
    *len = queryBar->writePos;
}

void QueryBar_SetEntry(QueryBar *queryBar, View *view, char *str, uint len){
    if(QueryBar_AcceptInput(queryBar, str, len)){
        uint p = queryBar->writePosU8;
        uint rawP = queryBar->writePos;
        Buffer_RemoveRangeRaw(&queryBar->buffer, rawP, queryBar->buffer.taken);
        rawP += Buffer_InsertStringAt(&queryBar->buffer, p, str, len, 0);
        queryBar->cursor.textPosition.y =
            Buffer_Utf8RawPositionToPosition(&queryBar->buffer, rawP);
    }
}

int QueryBar_AddEntry(QueryBar *queryBar, View *view, char *str, uint len){
    AssertA(queryBar != nullptr && str != nullptr && len > 0,
            "Invalid QueryBar pointer");
    
    if(QueryBar_AcceptInput(queryBar, str, len)){
        uint p = queryBar->cursor.textPosition.y;
        uint rawP = Buffer_Utf8PositionToRawPosition(&queryBar->buffer, p);
        rawP += Buffer_InsertStringAt(&queryBar->buffer, p, str, len, 0);
        
        queryBar->cursor.textPosition.y =
            Buffer_Utf8RawPositionToPosition(&queryBar->buffer, rawP);
        return QueryBar_DynamicUpdate(queryBar, view);
    }
    
    return 0;
}

int QueryBar_RemoveOne(QueryBar *queryBar, View *view){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    uint pos = queryBar->cursor.textPosition.y;
    if(pos > queryBar->writePosU8){
        Buffer_RemoveRange(&queryBar->buffer, pos-1, pos);
        pos -= 1;
        queryBar->cursor.textPosition.y = pos;
        return QueryBar_DynamicUpdate(queryBar, view);
    }
    
    return 0;
}

void QueryBar_RemoveAllFromCursor(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    uint pos = queryBar->cursor.textPosition.y;
    if(pos > queryBar->writePosU8){
        Buffer_RemoveRange(&queryBar->buffer, queryBar->writePosU8, pos);
        queryBar->cursor.textPosition.y = queryBar->writePosU8;
        //NOTE: Since this one clears the query bar we dont need to actually check for updates
    }
}

int QueryBar_NextItem(QueryBar *queryBar, View *view){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    int r = 0;
    switch(queryBar->cmd){
        case QUERY_BAR_CMD_SEARCH:{
            BufferView *bView = View_GetBufferView(view);
            DoubleCursor cursor;
            cursor.textPosition = vec2ui(queryBar->searchCmd.lineNo,
                                         queryBar->searchCmd.position+1);
            r = QueryBarCommandSearch(queryBar, bView->lineBuffer, &cursor);
        } break;
        case QUERY_BAR_CMD_REVERSE_SEARCH:{
            BufferView *bView = View_GetBufferView(view);
            DoubleCursor cursor;
            cursor.textPosition = vec2ui(queryBar->searchCmd.lineNo,
                                         queryBar->searchCmd.position+
                                         queryBar->searchCmd.length);
            
            QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_SEARCH);
            r = QueryBarCommandSearch(queryBar, bView->lineBuffer, &cursor);
        } break;
        default:{}
    }
    
    return r;
}

int QueryBar_PreviousItem(QueryBar *queryBar, View *view){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    int r = 0;
    switch(queryBar->cmd){
        case QUERY_BAR_CMD_SEARCH:{
            BufferView *bView = View_GetBufferView(view);
            DoubleCursor cursor;
            cursor.textPosition = vec2ui(queryBar->searchCmd.lineNo,
                                         queryBar->searchCmd.position);
            
            QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_REVERSE_SEARCH);
            r = QueryBarCommandSearch(queryBar, bView->lineBuffer, &cursor);
        } break;
        case QUERY_BAR_CMD_REVERSE_SEARCH:{
            uint line, p;
            BufferView *bView = View_GetBufferView(view);
            DoubleCursor cursor;
            line = queryBar->searchCmd.lineNo;
            if(queryBar->searchCmd.position > 0){
                p = queryBar->searchCmd.position-1;
            }else if(line > 0){
                line--;
                Buffer *buf = BufferView_GetBufferAt(bView, line);
                p = buf->taken-1;
            }else{
                return 0;
            }
            
            cursor.textPosition = vec2ui(line, p);
            r = QueryBarCommandSearch(queryBar, bView->lineBuffer, &cursor);
        } break;
        default:{}
    }
    
    return r;
}

QueryBarCommand QueryBar_GetActiveCommand(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    return queryBar->cmd;
}

void QueryBar_GetSearchResult(QueryBar *queryBar, QueryBarCmdSearch **result){
    *result = &queryBar->searchCmd;
}

int QueryBar_Reset(QueryBar *queryBar, View *view, int commit){
    BufferView *bView = View_GetBufferView(view);
    int r = 1;
    if(commit == 0){
        if(queryBar->cmd == QUERY_BAR_CMD_CUSTOM){
            if(queryBar->cancelCallback){
                r = queryBar->cancelCallback(queryBar, view);
            }
        }else{
            BufferView_CursorToPosition(bView, queryBar->cursorBackup.x,
                                        queryBar->cursorBackup.y);
        }
    }else{
        switch(queryBar->cmd){
            case QUERY_BAR_CMD_JUMP_TO_LINE:{
                char *number = nullptr;
                uint nLen = 0;
                QueryBar_GetWrittenContent(queryBar, &number, &nLen);
                if(nLen > 0){
                    uint target = StringToUnsigned(number, nLen);
                    uint maxn = BufferView_GetLineCount(bView);
                    target = target > 0 ? target-1 : 0;
                    target = Clamp(target, 0, maxn-1);
                    Buffer *buffer = BufferView_GetBufferAt(bView, target);
                    uint p = Buffer_FindFirstNonEmptyLocation(buffer);
                    BufferView_CursorToPosition(bView, target, p);
                }
            } break;
            case QUERY_BAR_CMD_SEARCH:
            case QUERY_BAR_CMD_REVERSE_SEARCH:{
                if(queryBar->searchCmd.valid){
                    Buffer *buf = BufferView_GetBufferAt(bView, queryBar->searchCmd.lineNo);
                    uint p = Buffer_Utf8RawPositionToPosition(buf,
                                                              queryBar->searchCmd.position);
                    BufferView_CursorToPosition(bView, queryBar->searchCmd.lineNo, p);
                }
            } break;
            case QUERY_BAR_CMD_CUSTOM:{
                if(queryBar->commitCallback){
                    r = queryBar->commitCallback(queryBar, view);
                }
            } break;
            default:{}
        }
    }
    
    if(r != 0){
        queryBar->cmd = QUERY_BAR_CMD_NONE;
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->buffer.count);
        queryBar->writePosU8 = 0;
        queryBar->writePos = 0;
        queryBar->cursor.textPosition = vec2ui(0,0);
        queryBar->cursorBackup = vec2ui(0, 0);
        queryBar->entryCallback = nullptr;
        queryBar->cancelCallback = nullptr;
        queryBar->commitCallback = nullptr;
        queryBar->filter = INPUT_FILTER_INITIALIZER;
    }
    
    return r;
}
