#include <query_bar.h>
#include <view.h>
#include <bufferview.h>
#include <app.h>
#include <parallel.h>
#include <fstream>
#include <utilities.h>
#include <storage.h>

#define QUERY_BAR_HISTORY_PATH ".history"

static void QueryBar_StartCommand(QueryBar *queryBar, QueryBarCommand cmd,
                                  char *name=nullptr, uint titleLen=0);

static int QueryBar_EmptySearchReplaceCallback(QueryBar *bar, View *view, int accepted){
    (void)bar; (void)view; (void)accepted;
    printf("Warning unconfigured search callback\n");
    //printf("Accepted: %d\n", accepted);
    return 0;
}

std::string QueryBarHistory_GetPath(){
    std::string path(AppGetConfigDirectory());
    if(path[path.size()-1] != '/' && path[path.size()-1] != '\\'){
        path += SEPARATOR_STRING;
    }
    path += QUERY_BAR_HISTORY_PATH;
    return path;
}

void AppQueryBarSearchJumpToResult(QueryBar *bar, View *view);
// called when user pressed enter and the query bar is in SearchAndReplace mode
static int QueryBar_SearchAndReplaceProcess(QueryBar *queryBar, View *view, int fromEnter=0){
    int r = 0;
    char *search = nullptr;
    uint searchLen = 0;
    QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;

    QueryBar_GetWrittenContent(queryBar, &search, &searchLen);

    if(searchLen > 0 || fromEnter){
        if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_SEARCH && searchLen > 0){
            AssertA(searchLen < sizeof(replace->toLocate), "Query is too big");

            Memcpy(replace->toLocate, search, searchLen);
            replace->toLocateLen = searchLen;
            replace->toLocate[searchLen] = 0;

            replace->state = QUERY_BAR_SEARCH_AND_REPLACE_REPLACE;
            QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_SEARCH_AND_REPLACE);

        }else if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_REPLACE && searchLen > 0){
            AssertA(searchLen < sizeof(replace->toReplace), "Query is too big");

            Memcpy(replace->toReplace, search, searchLen);
            replace->toReplaceLen = searchLen;
            replace->toReplace[searchLen] = 0;

            replace->state = QUERY_BAR_SEARCH_AND_REPLACE_EXECUTE;
        }else if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_REPLACE){
            // this is actually an erase
            replace->toReplaceLen = 0;
            replace->toReplace[0] = 0;
            replace->state = QUERY_BAR_SEARCH_AND_REPLACE_EXECUTE;
        }else if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_ASK){
            // TODO: This is a OK on the execute question we need to replace the content
            int toNext = 1;
            if(fromEnter){
                replace->searchCallback(queryBar, view, 1);
            }else{
                if(search[0] == 'y' || search[0] == 'Y'){
                    replace->searchCallback(queryBar, view, 1);
                }else if(search[0] == 'n' || search[0] == 'N'){
                    replace->searchCallback(queryBar, view, 0);
                    queryBar->searchCmd.position += Max(1, replace->toLocateLen);
                }else{
                    toNext = 0;
                    r = 0;
                    QueryBar_SetEntry(queryBar, view, nullptr, 0);
                }
            }
            if(toNext)
                replace->state = QUERY_BAR_SEARCH_AND_REPLACE_EXECUTE;
        }

        if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_EXECUTE){
            DoubleCursor cursor;
            QueryBarCmdSearch searchResult;
            BufferView *bView = View_GetBufferView(view);
            if(queryBar->searchCmd.valid){
                uint location = queryBar->searchCmd.position;
                uint newSize  = queryBar->replaceCmd.toReplaceLen;
                uint newPos = location + newSize;
                cursor.textPosition = vec2ui(queryBar->searchCmd.lineNo, newPos);
            }else{
                cursor = bView->sController.cursor;
            }
            int s = QueryBarCommandSearch(queryBar, bView->lineBuffer, &cursor,
                                          replace->toLocate, replace->toLocateLen);

            searchResult = queryBar->searchCmd;
            if(s > 0){
                AppQueryBarSearchJumpToResult(queryBar, view);
                replace->state = QUERY_BAR_SEARCH_AND_REPLACE_ASK;
                QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_SEARCH_AND_REPLACE);
            }

            queryBar->searchCmd = searchResult;
            r = s > 0 ? 0 : 1;
        }
    }else{
        r = 1;
    }
    return r;
}

void QueryBar_SetInteractiveSearchCallback(QueryBar *queryBar, OnInteractiveSearch onConfirm){
    QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
    replace->searchCallback = onConfirm;
}

static int QueryBar_AcceptInput(QueryBar *queryBar, char *input, uint len, int force){
    QueryBarInputFilter *filter = &queryBar->filter;
    if(filter->digitOnly){
        return StringIsDigits(input, len);
    }

    if(force)
        return 1;

    return filter->allowFreeType ? 1 : 0;
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

void QueryBar_SetPendingCall(QueryBar *queryBar, char *title, uint size,
                             OnQueryBarEntry entry, OnQueryBarCancel cancel,
                             OnQueryBarCommit commit, QueryBarInputFilter *filter)
{
    queryBar->hasPendingCallbacks = true;
    queryBar->pushedEntryCallback = entry;
    queryBar->pushedCancelCallback = cancel;
    queryBar->pushedCommitCallback = commit;
    queryBar->pendingSize = (uint)Min((Float)size, (Float)64);
    if(filter) queryBar->pendingFilter = *filter;
    memcpy(queryBar->pendingTitle, title, queryBar->pendingSize);
}

/* Sets tittle and ids */
static void QueryBar_StartCommand(QueryBar *queryBar, QueryBarCommand cmd,
                                  char *name, uint titleLen)
{
    char title[32];
    uint len = 0;
    uint lastp = queryBar->cursor.textPosition.y - queryBar->writePosU8;
    queryBar->searchCmd.valid = 0;
    queryBar->filter.digitOnly = 0;
    queryBar->filter.toHistory = false;
    queryBar->filter.allowCursorJump = false;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    queryBar->pushedEntryCallback = nullptr;
    queryBar->pushedCancelCallback = nullptr;
    queryBar->pushedCommitCallback = nullptr;
    queryBar->hasPendingCallbacks = false;
    queryBar->pendingSize = 0;
    queryBar->pendingFilter = INPUT_FILTER_INITIALIZER;
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
        case QUERY_BAR_CMD_INTERACTIVE:
        case QUERY_BAR_CMD_CUSTOM:{
            if(title && titleLen > 0){
                len = snprintf(title, sizeof(title), "%s: ", name);
            }else{
                len = snprintf(title, sizeof(title), ": ");
            }
        } break;
        case QUERY_BAR_CMD_SEARCH_AND_REPLACE:{
            QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
            if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_SEARCH){
                len = snprintf(title, sizeof(title), "Search and Replace (Search): ");
            }else if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_REPLACE){
                len = snprintf(title, sizeof(title), "Search and Replace (Replace): ");
            }else if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_ASK){
                len = snprintf(title, sizeof(title), "Replace (y/n) ? ");
            }else{
                AssertA(0, "Invalid state");
            }
        } break;
        default:{
            AssertErr(0, "Command not implemented");
        }
    }

    if(!QueryBar_AreCommandsContinuous(queryBar, cmd)){
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->buffer.count, &queryBar->encoder);
        lastp = 0;
    }else{
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->writePosU8, &queryBar->encoder);
    }

    queryBar->writePos = Buffer_InsertStringAt(&queryBar->buffer, 0, title, len, &queryBar->encoder);
    queryBar->writePosU8 =
            Buffer_Utf8RawPositionToPosition(&queryBar->buffer, queryBar->writePos,
                                             &queryBar->encoder);
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
        case QUERY_BAR_CMD_SEARCH_AND_REPLACE:{
            QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
            if(replace->state == QUERY_BAR_SEARCH_AND_REPLACE_ASK){
                r = QueryBar_SearchAndReplaceProcess(queryBar, view);
                if(r != 0){
                    r = -2;
                }
            }
        } break;
        case QUERY_BAR_CMD_INTERACTIVE:
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
    QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
    Buffer_Free(&queryBar->buffer);
    queryBar->isActive = 0;
    queryBar->writePos = 0;
    queryBar->writePosU8 = 0;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    queryBar->cmd = QUERY_BAR_CMD_NONE;
    replace->toLocateLen = 0;
    replace->toReplaceLen = 0;
    replace->searchCallback = QueryBar_EmptySearchReplaceCallback;
}

void QueryBar_ClearHiddenCharacters(QueryBar *queryBar){
    queryBar->filter.hideChars = false;
    queryBar->pendingMemAddr = 0;
    memset(queryBar->pendingMemory, 0, sizeof(queryBar->pendingMemory));
}

/*
* Sets the query bar to null values.
*/
void QueryBar_Initialize(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
    queryBar->buffer = BUFFER_INITIALIZER;
    queryBar->qHistoryAt = -1;
    queryBar->isActive = 0;
    queryBar->writePos = 0;
    queryBar->writePosU8 = 0;
    queryBar->entryCallback = nullptr;
    queryBar->cancelCallback = nullptr;
    queryBar->commitCallback = nullptr;
    queryBar->cmd = QUERY_BAR_CMD_NONE;
    replace->toLocateLen = 0;
    replace->toReplaceLen = 0;
    replace->searchCallback = QueryBar_EmptySearchReplaceCallback;
    queryBar->filter.toHistory = false;
    queryBar->filter.allowCursorJump = false;
    Buffer_Init(&queryBar->buffer, DefaultAllocatorSize);
    EncoderDecoder_InitFor(&queryBar->encoder, ENCODER_DECODER_UTF8);

    QueryBar_ClearHiddenCharacters(queryBar);
}

/* Moves cursor left */
void QueryBar_MoveLeft(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    if(queryBar->filter.allowCursorJump){
        queryBar->cursor.textPosition.y = Max(queryBar->cursor.textPosition.y - 1,
                                              queryBar->writePosU8);
    }
}

/* Moves cursor right */
void QueryBar_MoveRight(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    if(queryBar->filter.allowCursorJump){
        queryBar->cursor.textPosition.y = Min(queryBar->cursor.textPosition.y + 1,
                                              queryBar->buffer.count);
    }
}

void QueryBar_JumpToPrevious(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    if(queryBar->filter.allowCursorJump){
        uint u8 = queryBar->cursor.textPosition.y;
        u8 = Buffer_FindPreviousWordU8(&queryBar->buffer, u8, QUERY_BAR_DEFAULT_JUMP_STR,
                                       QUERY_BAR_DEFAULT_JUMP_STR_LEN, &queryBar->encoder);
        queryBar->cursor.textPosition.y = Max(u8, queryBar->writePosU8);
    }
}

void QueryBar_JumpToNext(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    if(queryBar->filter.allowCursorJump){
        uint u8 = queryBar->cursor.textPosition.y;
        u8 = Buffer_FindNextWord8(&queryBar->buffer, u8, QUERY_BAR_DEFAULT_JUMP_STR,
                                  QUERY_BAR_DEFAULT_JUMP_STR_LEN, &queryBar->encoder);
        queryBar->cursor.textPosition.y = Min(u8, queryBar->buffer.count);
    }
}

/* Sets query bar geometry */
void QueryBar_SetGeometry(QueryBar *queryBar, Geometry *geometry, Float lineHeight){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->geometry.lower = geometry->lower;
    queryBar->geometry.upper = vec2ui(geometry->upper.x, geometry->lower.y + lineHeight);
    queryBar->geometry.extensionX = geometry->extensionX;
    queryBar->geometry.extensionY = geometry->extensionY;
}

void QueryBar_EnableInputToHistory(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->filter.toHistory = true;
}

void QueryBar_EnableCursorJump(QueryBar *queryBar){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    queryBar->filter.allowCursorJump = true;
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

void QueryBar_ActiveCustomFull(QueryBar *queryBar, char *title, uint titlelen,
                               OnQueryBarEntry entry, OnQueryBarCancel cancel,
                               OnQueryBarCommit commit, QueryBarInputFilter *filter,
                               QueryBarCommand cmd)
{
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    QueryBar_StartCommand(queryBar, cmd, title, titlelen);

    queryBar->entryCallback  = entry;
    queryBar->cancelCallback = cancel;
    queryBar->commitCallback = commit;
    if(filter) queryBar->filter = *filter;
    else{
        queryBar->filter.toHistory = false;
        queryBar->filter.allowCursorJump = false;
    }
}

void QueryBar_SetFilter(QueryBar *queryBar, QueryBarInputFilter *filter){
    if(filter && queryBar) queryBar->filter = *filter;
}

void QueryBar_ActivateCustom(QueryBar *queryBar, char *title, uint titlelen,
                             OnQueryBarEntry entry, OnQueryBarCancel cancel,
                             OnQueryBarCommit commit, QueryBarInputFilter *filter)
{
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    // need a copy in case it is a re-load of the querybar
    QueryBarInputFilter filterCopy;
    if(filter){
        filterCopy = *filter;
    }

    QueryBar_StartCommand(queryBar, QUERY_BAR_CMD_CUSTOM, title, titlelen);

    queryBar->entryCallback  = entry;
    queryBar->cancelCallback = cancel;
    queryBar->commitCallback = commit;

    // ????
    if(filter) queryBar->filter = filterCopy;
    else{
        queryBar->filter.toHistory = false;
        queryBar->filter.allowCursorJump = false;
    }
}

void QueryBar_GetWrittenContent(QueryBar *queryBar, char **ptr, uint *len){
    AssertA(queryBar != nullptr && ptr != nullptr && len != nullptr,
            "Invalid QueryBar pointer");

    *ptr = &queryBar->buffer.data[queryBar->writePos];
    *len = queryBar->buffer.taken - queryBar->writePos;
}

uint QueryBar_GetRenderContent(QueryBar *queryBar, std::string &str){
    std::vector<std::string> splitted;
    char *content = nullptr;
    uint contentLen = 0;
    uint diff = 0;
    int compression = AppGetPathCompression();
    QueryBar_GetWrittenContent(queryBar, &content, &contentLen);

    if(compression < 0){
        str = std::string(content, contentLen);
        return 0;
    }

    std::string possiblePaths;
    for(uint i = 0; i < contentLen; i++){
        if(content[i] != ' '){
            possiblePaths = std::string(&content[i], contentLen-i);
            break;
        }
    }

    StringSplit(possiblePaths, splitted, SEPARATOR_CHAR);
    if((int)splitted.size() > compression){
        bool is_folder = content[contentLen-1] == SEPARATOR_CHAR;
        str = " .." SEPARATOR_STRING;
        for(uint i = splitted.size()-compression-1; i < splitted.size(); i++){
            str += splitted[i];
            if(i < splitted.size() - 1) str += SEPARATOR_STRING;
        }
        if(is_folder) str += SEPARATOR_STRING;
        diff = contentLen - str.size();
    }else{
        str = std::string(content, contentLen);
    }

    return diff;
}

void QueryBar_GetTitle(QueryBar *queryBar, char **ptr, uint *len){
    AssertA(queryBar != nullptr && ptr != nullptr && len != nullptr,
            "Invalid QueryBar pointer");
    *ptr = queryBar->buffer.data;
    *len = queryBar->writePos;
}

void QueryBar_SetEntry(QueryBar *queryBar, View *view, char *str, uint len){
    if(QueryBar_AcceptInput(queryBar, str, len, 1)){
        uint p = queryBar->writePosU8;
        uint rawP = queryBar->writePos;
        Buffer_RemoveRangeRaw(&queryBar->buffer, rawP, queryBar->buffer.taken,
                              &queryBar->encoder);
        if(len > 0){
            rawP += Buffer_InsertStringAt(&queryBar->buffer, p, str, len, &queryBar->encoder);
        }

        queryBar->cursor.textPosition.y =
                        Buffer_Utf8RawPositionToPosition(&queryBar->buffer, rawP, &queryBar->encoder);
    }
}

int QueryBar_AddEntry(QueryBar *queryBar, View *view, char *str, uint len){
    AssertA(queryBar != nullptr && str != nullptr && len > 0,
            "Invalid QueryBar pointer");

    if(QueryBar_AcceptInput(queryBar, str, len, 0)){
        uint p = queryBar->cursor.textPosition.y;
        uint rawP = Buffer_Utf8PositionToRawPosition(&queryBar->buffer, p,
                                                nullptr, &queryBar->encoder);
        if(queryBar->filter.hideChars){
            std::string tmp(len, '*');
            rawP += Buffer_InsertStringAt(&queryBar->buffer, p, tmp.data(), len,
                                          &queryBar->encoder);

            if((queryBar->pendingMemAddr + len) < sizeof(queryBar->pendingMemory)){
                strncat(queryBar->pendingMemory, str, len);
                queryBar->pendingMemAddr += len;
            }else{
                printf("oops, cannot hold more data!\n");
            }
        }else{
            rawP += Buffer_InsertStringAt(&queryBar->buffer, p, str, len,
                                          &queryBar->encoder);
        }

        queryBar->cursor.textPosition.y =
            Buffer_Utf8RawPositionToPosition(&queryBar->buffer, rawP, &queryBar->encoder);
        return QueryBar_DynamicUpdate(queryBar, view);
    }

    return 0;
}

int QueryBar_RemoveOne(QueryBar *queryBar, View *view){
    AssertA(queryBar != nullptr, "Invalid QueryBar pointer");
    uint pos = queryBar->cursor.textPosition.y;
    if(pos > queryBar->writePosU8){
        Buffer_RemoveRange(&queryBar->buffer, pos-1, pos, &queryBar->encoder);
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
        Buffer_RemoveRange(&queryBar->buffer, queryBar->writePosU8, pos, &queryBar->encoder);
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
        case QUERY_BAR_CMD_INTERACTIVE:{
            QueryBarHistory *qHistory = AppGetQueryBarHistory();
            if(queryBar->qHistoryAt > 0){
                uint where = queryBar->qHistoryAt-1;
                QueryBarHistoryItem *item = CircularStack_At(qHistory->history, where);
                if(item){
                    char *data = (char *)item->value.c_str();
                    uint len = item->value.size();
                    QueryBar_SetEntry(queryBar, view, data, len);
                    queryBar->qHistoryAt = where;
                }
            }else if(queryBar->qHistoryAt == 0){
                queryBar->qHistoryAt = -1;
                QueryBar_RemoveAllFromCursor(queryBar);
            }
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
        case QUERY_BAR_CMD_INTERACTIVE:{
            QueryBarHistory *qHistory = AppGetQueryBarHistory();
            uint where = queryBar->qHistoryAt+1;
            QueryBarHistoryItem *item = CircularStack_At(qHistory->history, where);
            if(item){
                char *data = (char *)item->value.c_str();
                uint len = item->value.size();
                QueryBar_SetEntry(queryBar, view, data, len);
                queryBar->qHistoryAt = where;
            }
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
        if(queryBar->cmd == QUERY_BAR_CMD_CUSTOM ||
           queryBar->cmd == QUERY_BAR_CMD_INTERACTIVE)
        {
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
                    target = Clamp(target, (uint)0, maxn-1);
                    Buffer *buffer = BufferView_GetBufferAt(bView, target);
                    uint p = Buffer_FindFirstNonEmptyLocation(buffer);
                    BufferView_CursorToPosition(bView, target, p);
                }
            } break;
            case QUERY_BAR_CMD_SEARCH_AND_REPLACE:{
                r = QueryBar_SearchAndReplaceProcess(queryBar, view, 1);
            } break;
            case QUERY_BAR_CMD_SEARCH:
            case QUERY_BAR_CMD_REVERSE_SEARCH:{
                if(queryBar->searchCmd.valid){
                    Buffer *buf = BufferView_GetBufferAt(bView, queryBar->searchCmd.lineNo);
                    uint p =
                        Buffer_Utf8RawPositionToPosition(buf, queryBar->searchCmd.position,
                                                         &queryBar->encoder);
                    BufferView_CursorToPosition(bView, queryBar->searchCmd.lineNo, p);
                }
            } break;
            case QUERY_BAR_CMD_INTERACTIVE:
            case QUERY_BAR_CMD_CUSTOM:{
                if(queryBar->commitCallback){
                    char *content = nullptr;
                    uint size = 0;
                    QueryBar_GetWrittenContent(queryBar, &content, &size);
                    std::string data = std::string(content, size);

                    r = queryBar->commitCallback(queryBar, view);
                    if(r != 0 && queryBar->filter.toHistory){
                        std::string path;
                        QueryBarHistory *qHistory = AppGetQueryBarHistory();
                        if(content && size > 0){
                            bool insert = true;
                            if(CircularStack_Size(qHistory->history) > 0){
                                QueryBarHistoryItem *top =
                                                CircularStack_At(qHistory->history, 0);
                                insert = data != top->value;
                            }

                            if(insert){
                                path = QueryBarHistory_GetPath();
                                QueryBarHistoryItem item;
                                item.value = data;
                                CircularStack_Push(qHistory->history, &item);
                                QueryBarHistory_DetachedStore(qHistory, path.c_str());
                            }
                        }
                    }
                }
            } break;
            default:{}
        }
    }

    if(r != 0){
        QueryBarCmdSearchAndReplace *replace = &queryBar->replaceCmd;
        queryBar->cmd = QUERY_BAR_CMD_NONE;
        Buffer_RemoveRange(&queryBar->buffer, 0, queryBar->buffer.count, &queryBar->encoder);
        queryBar->writePosU8 = 0;
        queryBar->writePos = 0;
        queryBar->qHistoryAt = -1;
        queryBar->cursor.textPosition = vec2ui(0,0);
        queryBar->cursorBackup = vec2ui(0, 0);
        queryBar->entryCallback = nullptr;
        queryBar->cancelCallback = nullptr;
        queryBar->commitCallback = nullptr;
        queryBar->filter = INPUT_FILTER_INITIALIZER;
        replace->toLocateLen = 0;
        replace->toReplaceLen = 0;
        replace->searchCallback = QueryBar_EmptySearchReplaceCallback;

        if(queryBar->hasPendingCallbacks){
            QueryBar_ActivateCustom(queryBar, queryBar->pendingTitle, queryBar->pendingSize,
                                    queryBar->pushedEntryCallback,
                                    queryBar->pushedCancelCallback,
                                    queryBar->pushedCommitCallback,
                                    &queryBar->pendingFilter);
            View_ReturnToState(AppGetActiveView(), View_QueryBar);
            QueryBar_EnableCursorJump(queryBar);
            r = 0;
        }
    }

    return r;
}

void QueryBarHistory_LineProcessor(char **p, uint size, uint lineNr, uint at,
                                   uint total, void *priv)
{
    QueryBarHistoryItem item;
    QueryBarHistory *qHistory = (QueryBarHistory *)priv;

    item.value = std::string(*p, size-1);
    CircularStack_Push(qHistory->history, &item);
}

void QueryBarHistory_DetachedLoad(QueryBarHistory *history, const char *basePath){
    std::string path(basePath);
    QueryBarHistory *qHistory = history;
    char *content = nullptr;
    uint fileSize = 0;
    StorageDevice *device = FetchStorageDevice();

    if(device){
        content = device->GetContentsOf(basePath, &fileSize);
    }

    if(content && fileSize > 0){
    #if 0
        ReadStringLineByLine(content, fileSize, [&](char *line, uint len) -> void{
            if(len > 0){
                QueryBarHistoryItem item;
                item.value = std::string(line);
                CircularStack_Push(qHistory->history, &item);
            }
        });

        AllocatorFree(content);
    #else
        Lex_LineProcess(content, fileSize, QueryBarHistory_LineProcessor,
                        0, qHistory, true);
    #endif
    }else if(content){
        AllocatorFree(content);
    }
}

void QueryBarHistory_DetachedStore(QueryBarHistory *_history, const char *basePath){
    FileHandle file;
    std::string path(basePath);
    QueryBarHistory *qHistory = _history;

    StorageDevice *storage = FetchStorageDevice();
    CircularStack<QueryBarHistoryItem> *history = qHistory->history;
    if(!storage->StreamWriteStart(&file, basePath)){
        //printf("Could not open file %s in current storage device\n", basePath);
        return;
    }

    for(uint i = 0; i < CircularStack_Size(history); i++){
        QueryBarHistoryItem *item = CircularStack_At(history, i);
        if(item->value.size() > 0){
            // Raw value needs '\n' so we can easily parse it later
            std::string rawValue = item->value + std::string("\n");
            storage->StreamWriteString(&file, rawValue.c_str());
            //printf(" Writing %s\n", item->value.c_str());
        }
    }

    storage->StreamFinish(&file);
}
