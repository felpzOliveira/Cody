#include <dbgapp.h>
#include <app.h>
#include <graphics.h>
#include <keyboard.h>

#define MODULE_NAME "DBG-APP"
#define StateChangeMask  (1 << 0)
#define BkptFeedbackMask (1 << 1)
#define ExpFeedbackMask  (1 << 2)

typedef enum{
    SyncStop=0, SyncExit, SyncState, SyncBkptFeedback, SyncExpFeedback
}DbgSyncCode;

/* template this ? */
struct DbgSyncPackage{
    DbgSyncCode code;
    DbgStop sp;
    DbgState state;
    BreakpointFeedback bkptFed;
    ExpressionFeedback expFed;
};

struct DbgLineBufferPair{
    LineBuffer *lineBuffer;
    bool state;
};

template<typename Fn>
struct DbgUserCallback{
    Fn *fn;
    void *fn_priv;
    uint handle;
};

template<typename Fn>
struct DbgCallbackList{
    std::vector<Fn> callbacks;
};

template<typename Fn>
void DbgApp_UnregisterHandle(DbgCallbackList<DbgUserCallback<Fn>> *list, uint handle){
    bool found = false;
    for(auto it = list->callbacks.begin(); it != list->callbacks.end(); it++){
        if(it->handle == handle){
            list->callbacks.erase(it);
            found = true;
            break;
        }
    }

    if(!found){
        BUG();
        printf("Did not find corresponding handle for %u\n", handle);
    }
}

template<typename Fn>
uint DbgApp_RegisterRoutine(DbgCallbackList<DbgUserCallback<Fn>> *list,
                            Fn fn, void *priv, uint mask)
{
    uint handle = 0;
    bool done = false;
    do{
        handle = Bad_RNG16() | (mask << 16);
        done = true;
        for(auto cb : list->callbacks){
            if(handle == cb.handle){
                done = false;
                break;
            }
        }
    }while(!done);
    list->callbacks.push_back({ .fn = fn, .fn_priv = priv, .handle = handle });
    return handle;
}

typedef DbgUserCallback<DbgApp_UserStateReport> DbgStateChangeCallback;
typedef DbgCallbackList<DbgStateChangeCallback> DbgStateChangeList;

typedef DbgUserCallback<DbgApp_UserBkptFeedback> DbgBkptFeedbackCallback;
typedef DbgCallbackList<DbgBkptFeedbackCallback> DbgBkptFeedbackList;

typedef DbgUserCallback<DbgApp_UserExpressionFeedback> DbgExpFeedbackCallback;
typedef DbgCallbackList<DbgExpFeedbackCallback> DbgExpFeedbackList;

struct DbgSynchronizer{
    View *targetView;
    LineBuffer *dbgLB;
    bool is_stopped;
    DbgStateChangeList stateChangeList;
    DbgBkptFeedbackList bkptFeedbackList;
    DbgExpFeedbackList expFeedbackList;
    std::vector<LineBuffer *> loadedLBs;
    std::vector<DbgLineBufferPair> lockedLBs;
    ConcurrentQueue<DbgSyncPackage> inQ;
    BindingMap *mapping;
    BindingMap *lastMapping;
    // outQ can be directly accessed through dbg API with Dbg_SendPackage
};

/* Asynchronous handler */
DbgSynchronizer dbgSync = {
    .targetView = nullptr,
    .dbgLB = nullptr,
};
void DbgApp_AsyncHandleStopPoint(DbgStop *sp){
    dbgSync.inQ.push({.code = DbgSyncCode::SyncStop, .sp = *sp});
}

void DbgApp_AsyncHandleState(DbgState state){
    DbgStop empty = {.reason = No_Stop};
    dbgSync.inQ.push({.code = DbgSyncCode::SyncState, .sp = empty, .state = state});
}

void DbgApp_AsyncHandleExit(){
    dbgSync.inQ.push({.code = DbgSyncCode::SyncExit,});
}

void DbgApp_AsyncHandleBkptFeedback(BreakpointFeedback feedback){
    DbgSyncPackage pkg;
    pkg.code = DbgSyncCode::SyncBkptFeedback;
    pkg.bkptFed = feedback;
    dbgSync.inQ.push(pkg);
}

void DbgApp_AsyncHandleExpFeedback(ExpressionFeedback feedback){
    DbgSyncPackage pkg;
    pkg.code = DbgSyncCode::SyncExpFeedback;
    pkg.expFed = feedback;
    dbgSync.inQ.push(pkg);
}

/* Synchronous handler */
void DbgApp_RestoreViewStates(){
    ViewTree_ForAllViews([&](ViewNode *node) -> int{
        View *view = node->view;
        BufferView *bv = View_GetBufferView(view);
        if(!bv) return 0;
        LineBuffer *vlb = BufferView_GetLineBuffer(bv);
        if(!vlb) return 0;

        if(vlb == dbgSync.dbgLB){
            if(dbgSync.dbgLB){
                BufferView_SetViewType(bv, CodeView);
            }else{
                BufferView_SetViewType(bv, EmptyView);
            }
        }
        return 0;
    });
}

bool DbgApp_HandleStopPoint(DbgSyncPackage *pkg){
    DbgStop *sp = &pkg->sp;
    LineBuffer *dbgLB = nullptr;
    uint line = sp->line - 1; // remove +1
    int state = -1;
    dbgSync.is_stopped = true;
    auto adjust_cursor_fn = [&](BufferView *bView, uint linep,
                                int state, bool old) -> void
    {
        // since linebuffers don't hold cursor information we need
        // to perform computation on all bufferviews
        if(old){
            std::optional<LineHints> ohint = BufferView_GetLineHighlight(bView);
            if(ohint){
                linep = ohint.value().line;
            }
        }
        vec2ui vlines = BufferView_GetViewRange(bView);
        if(linep <= vlines.x || linep >= vlines.y){
            // if the line the debugger is going to is not in view
            // move everything to there
            Buffer *buffer = BufferView_GetBufferAt(bView, linep);
            uint p = Buffer_FindFirstNonEmptyLocation(buffer);
            BufferView_CursorToPosition(bView, linep, p);
        }

        // debugger uses the highlight line available in bufferview
        // to show which line it is looking at
        if(!old){
            LineHints hint = {linep, state};
            BufferView_SetHighlightLine(bView, hint);
            BufferView_SetViewType(bView, DbgView);
        }else{
            std::optional<LineHints> ohint = BufferView_GetLineHighlight(bView);
            if(ohint){
                LineHints hint = ohint.value();
                hint.state = state;
                BufferView_SetHighlightLine(bView, hint);
                BufferView_SetViewType(bView, DbgView);
            }
        }
    };

    if(sp->reason == Exit_Stop){
        return true;
    }

    // 1 - find the linebuffer corresponding to the stop point address
    //     but first restore if we did this before
    DbgApp_RestoreViewStates();

    // try the file provider first
    int r = FileProvider_FindByPath(&dbgLB, (char *)sp->file.c_str(),
                                    sp->file.size(), nullptr);
    if(!r){
        // if the file provider does not have it, than attempt to load it
        int fileType = -1;
        if(FileExists((char *)sp->file.c_str())){
            FileProvider_Load((char *)sp->file.c_str(),
                              sp->file.size(), fileType, &dbgLB);
            if(dbgLB){
                dbgSync.loadedLBs.push_back(dbgLB);
                DEBUG_MSG("Loaded %s\n", sp->file.c_str());
            }
        }
    }

    if(!dbgLB){
        // if we could not load the file this might be some obscure
        // libc file, we can't really look at source so return
        DEBUG_MSG("Failed to load %s ( stepping inside unknown source? )\n",
                  sp->file.c_str());
        // if the dbgSync.dbgLB was valid it means we are potentially
        // in a situation where a step entered somewhere we can't view
        // we can mark the line so that we have a visual hint
        if(dbgSync.dbgLB){
            // update all current views to show visual hint that
            // the debugger is inside a file that cannot be viewed
            state = sp->reason != Signal_Stop ? LHINT_STATE_UNKNOW : LHINT_STATE_INTERRUPT;

            ViewTree_ForAllViews([&](ViewNode *node) -> int{
                View *view = node->view;
                BufferView *bv = View_GetBufferView(view);
                if(!bv) return 0;
                LineBuffer *vlb = BufferView_GetLineBuffer(bv);
                if(!vlb) return 0;

                if(vlb == dbgSync.dbgLB){
                    adjust_cursor_fn(bv, 0, state, true);
                }
                return 0;
            });
            // we should be fine now, do one more
            View *view = dbgSync.targetView;
            BufferView *bview = View_GetBufferView(view);
            adjust_cursor_fn(bview, 0, state, true);

            AppSetActiveView(dbgSync.targetView, false);
        }
        return false;
    }


    // update the dbg linebuffer
    dbgSync.dbgLB = dbgLB;

    // 2 - we need to make the linebuffer not writtable, so we don't get
    //     some weird behaviours, but for that we need to keep track
    //     of the buffers so check the locked buffers and add it
    bool already_in = false;
    for(DbgLineBufferPair &p : dbgSync.lockedLBs){
        if(p.lineBuffer == dbgSync.dbgLB){
            already_in = true;
            break;
        }
    }

    if(!already_in){
        // add to the locked list
        bool lstate = LineBuffer_IsWrittable(dbgSync.dbgLB);
        dbgSync.lockedLBs.push_back({dbgSync.dbgLB, lstate});
    }

    // 3 - We need to pick a view to show this linebuffer (if not in view)
    //     here however lies a problem: because there can be multiple views
    //     in use for the same linebuffer if we don't update them all we
    //     can get weird stuff like one in DbgView while others are in CodeView
    //     and we would mix edit/debug inside the same linebuffer, not good.
    //     so we need to:
    //        a - compute a cursor position for the viewing;
    //        b - lock all bufferviews that are linked against this linebuffer;
    //        c - pick a view if no view holds this linebuffer and swap its state;
    //        d - update the cursor on all bufferviews;

    // assume we stopped inside our code and because we hit a good point
    // i.e.: breakpoint/next/step
    state = LHINT_STATE_VALID;
    if(sp->reason == Signal_Stop){
        // we stopped because of a signal crash/interrupt
        state = LHINT_STATE_INTERRUPT;
    }else if(sp->level > 0){
        // for some reason the stop point is not in our code
        // the debugger will send a frame that we have access
        // so we need to just mark it as not being the 0
        state = LHINT_STATE_UNKNOW;
    }

    ViewTree_ForAllViews([&](ViewNode *node) -> int{
        View *view = node->view;
        BufferView *bv = View_GetBufferView(view);
        if(!bv) return 0;
        LineBuffer *vlb = BufferView_GetLineBuffer(bv);
        if(!vlb) return 0;

        if(vlb == dbgSync.dbgLB){
            adjust_cursor_fn(bv, line, state, false);
        }
        return 0;
    });

    // we need to do one more to make sure the active view we picked
    // at the start of the process be updated.
    View *view = dbgSync.targetView;
    BufferView *bview = View_GetBufferView(view);
    BufferView_SwapBuffer(bview, dbgSync.dbgLB, DbgView);
    adjust_cursor_fn(bview, line, state, false);

    AppSetActiveView(dbgSync.targetView, false);

    DEBUG_MSG("Jumped to %d\n", (int)line);
    return false;
}

static void DbgApp_ClearLinebufferList(){
    for(DbgLineBufferPair &p : dbgSync.lockedLBs){
        LineBuffer_SetWrittable(p.lineBuffer, p.state);
    }
#if 0
    for(LineBuffer *lb : dbgSync.loadedLBs){
        // we need to inspect views and make them null
        // in case a loaded file is currently being displayed
        ViewTree_ForAllViews([&](ViewNode *node) -> int{
            View *view = node->view;
            BufferView *bv = View_GetBufferView(view);
            if(!bv) return 0;
            LineBuffer *vlb = BufferView_GetLineBuffer(bv);
            if(!vlb) return 0;

            if(vlb == lb){
                BufferView_SwapBuffer(bv, nullptr, EmptyView);
            }
            return 0;
        });

        FileProvider_Remove(lb);
        LineBuffer_Free(lb);
    }
#endif
    dbgSync.loadedLBs.clear();
    dbgSync.lockedLBs.clear();
}

static void DbgApp_Terminate(){
    DbgApp_RestoreViewStates();
    DbgApp_ClearLinebufferList();
    if(dbgSync.lastMapping){
        KeyboardSetActiveMapping(dbgSync.lastMapping);
        dbgSync.lastMapping = nullptr;
    }
    dbgSync.targetView = nullptr;

    DEBUG_MSG("Called termination\n");
}

template<typename Fn, typename ...Args>
void DbgApp_DispatchToUser(Args... args, DbgCallbackList<DbgUserCallback<Fn>> *fns){
    for(auto fn : fns->callbacks){
        fn.fn(args..., fn.fn_priv);
    }
}

void DbgApp_StateReport(DbgState state){
    DbgStateChangeList *list = &dbgSync.stateChangeList;
    DbgApp_DispatchToUser<DbgApp_UserStateReport, DbgState>(state, list);
}

void DbgApp_BkptFeedback(BreakpointFeedback bkptFed){
    DbgBkptFeedbackList *list = &dbgSync.bkptFeedbackList;
    DbgApp_DispatchToUser<DbgApp_UserBkptFeedback, BreakpointFeedback>(bkptFed, list);
}

void DbgApp_ExpressionFeedback(ExpressionFeedback expFed){
    DbgExpFeedbackList *list = &dbgSync.expFeedbackList;
    DbgApp_DispatchToUser<DbgApp_UserExpressionFeedback, ExpressionFeedback>(expFed, list);
}

bool DbgApp_PoolMessages(){
    bool exit = false;
    while(dbgSync.inQ.size() > 0){
        DbgSyncPackage pkg = dbgSync.inQ.pop();
        switch(pkg.code){
            case DbgSyncCode::SyncStop: {
                exit |= DbgApp_HandleStopPoint(&pkg);
            } break;
            case DbgSyncCode::SyncExit: {
                DbgApp_StateReport(DbgState::Exited);
                exit = true;
            } break;
            case DbgSyncCode::SyncState: {
                DbgApp_StateReport(pkg.state);
            } break;
            case DbgSyncCode::SyncExpFeedback: {
                DbgApp_ExpressionFeedback(pkg.expFed);
            } break;
            case DbgSyncCode::SyncBkptFeedback: {
                DbgApp_BkptFeedback(pkg.bkptFed);
            } break;
            default: {
                DEBUG_MSG("Unknown header\n");
            }
        }
    }

    if(exit){
        DbgApp_Terminate();
    }

    return !exit;
}

/* Keyboard stuff */
void DbgApp_DefaultEntry(char *utf8Data, int utf8Size, void *){
    (void)utf8Data; (void)utf8Size;
}

// TODO: It would be nice if all functions that talk to dbg stopped
//       after a Signal_Stop and only allow the exit command so that
//       we don't attempt next/continue/... (run maight be fine)
//       so that dbg<->UI can keep in sync.

void DbgApp_Run(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Run));
}

void DbgApp_Interrupt(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Interrupt));
}

void DbgApp_Next(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Next));
}

void DbgApp_Continue(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Continue));
}

void DbgApp_Step(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Step));
}

void DbgApp_Finish(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Finish));
}

void DbgApp_Eval(char *expression, uint handle){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(std::string(expression), (int)handle, DbgCode::Evaluate));
}

void DbgApp_SetKeyboard(){
    KeyboardSetActiveMapping(dbgSync.mapping);
}

void DbgApp_Break(const char *file, uint line){
    Dbg_SendPackage(DbgPackage(std::string(file), (int)line));
}

void DbgApp_Exit(){
    dbgSync.is_stopped = false;
    Dbg_SendPackage(DbgPackage(DbgCode::Exit));
}

void DbgApp_QueryBar(){
    AppCommandQueryBarInteractiveCommand();
    // we have to set the keyboard just in case we cancel the query bar
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_QueryBarPaste(){
    View *view = AppGetActiveView();
    if(View_IsQueryBarActive(view)){
        uint size = 0;
        const char *p = ClipboardGetString(&size);
        QueryBar *bar = View_GetQueryBar(view);
        int r = QueryBar_AddEntry(bar, view, (char *)p, size);
        if(r == 1){
            AppQueryBarSearchJumpToResult(bar, view);
        }else if(r == -2){
            AppDefaultReturn();
        }
    }
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_SwitchTheme(){
    AppCommandSwitchTheme();
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_RestoreWidgets(){
    DbgSupport_ResetWidgetsGeometry();
}

void DbgApp_SwitchBuffer(){
    AppCommandSwitchBuffer();
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_OpenFile(){
    AppCommandOpenFileWithViewType(DbgView, OPEN_FILE_FLAGS_NO_CREATION);
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_Search(){
    AppCommandQueryBarSearch();
    AppSetDelayedCall(DbgApp_SetKeyboard);
}

void DbgApp_Initialize(){
    BindingMap *mapping = KeyboardCreateMapping();
    RegisterKeyboardDefaultEntry(mapping, DbgApp_DefaultEntry, nullptr);

    RegisterRepeatableEvent(mapping, DbgApp_Run, Key_R);
    RegisterRepeatableEvent(mapping, DbgApp_Next, Key_N);
    RegisterRepeatableEvent(mapping, DbgApp_Step, Key_S);
    RegisterRepeatableEvent(mapping, DbgApp_Finish, Key_F);
    RegisterRepeatableEvent(mapping, DbgApp_Continue, Key_C);
    RegisterRepeatableEvent(mapping, DbgApp_Exit, Key_Escape);
    RegisterRepeatableEvent(mapping, DbgApp_QueryBar, Key_LeftControl, Key_Semicolon);
    RegisterRepeatableEvent(mapping, DbgApp_QueryBarPaste, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, DbgApp_SwitchTheme, Key_LeftControl, Key_T);
    RegisterRepeatableEvent(mapping, DbgApp_SwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, DbgApp_OpenFile, Key_LeftAlt, Key_F);
    RegisterRepeatableEvent(mapping, DbgApp_Search, Key_LeftControl, Key_S);
    RegisterRepeatableEvent(mapping, DbgApp_RestoreWidgets, Key_LeftControl, Key_R);

    // get the movement controls from App interface.
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

    dbgSync.mapping = mapping;
    dbgSync.lastMapping = nullptr;
}

void DbgApp_TakeKeyboard(){
    KeyboardSetActiveMapping(dbgSync.mapping);
}

void DbgApp_DelayedSave(){
    dbgSync.lastMapping = KeyboardGetActiveMapping();
    KeyboardSetActiveMapping(dbgSync.mapping);
}

bool DbgApp_IsStopped(){
    return dbgSync.is_stopped;
}

uint DbgApp_RegisterStateChangeCallback(DbgApp_UserStateReport *fn, void *priv){
    return DbgApp_RegisterRoutine<DbgApp_UserStateReport>(&dbgSync.stateChangeList,
                                                          fn, priv, StateChangeMask);
}

uint DbgApp_RegisterBreakpointFeedbackCallback(DbgApp_UserBkptFeedback *fn, void *priv){
    return DbgApp_RegisterRoutine<DbgApp_UserBkptFeedback>(&dbgSync.bkptFeedbackList,
                                                           fn, priv, BkptFeedbackMask);
}

uint DbgApp_RegisterExpressionFeedbackCallback(DbgApp_UserExpressionFeedback *fn, void *priv){
    return DbgApp_RegisterRoutine<DbgApp_UserExpressionFeedback>(&dbgSync.expFeedbackList,
                                                                 fn, priv, ExpFeedbackMask);
}

void DbgApp_UnregisterCallbackByHandle(uint handle){
    uint mask = handle >> 16;
    switch(mask){
        case StateChangeMask:{
            DbgApp_UnregisterHandle<DbgApp_UserStateReport>(&dbgSync.stateChangeList,
                                                            handle);
        } break;
        case BkptFeedbackMask:{
            DbgApp_UnregisterHandle<DbgApp_UserBkptFeedback>(&dbgSync.bkptFeedbackList,
                                                             handle);
        } break;
        case ExpFeedbackMask:{
            DbgApp_UnregisterHandle<DbgApp_UserExpressionFeedback>(&dbgSync.expFeedbackList,
                                                                   handle);
        } break;
        default:{
            BUG();
            printf("Unknow bit mask %d\n", (int)mask);
        }
    }
}

/* main entry point for calling the debugger */
void DbgApp_StartDebugger(const char *binaryPath, const char *args){
    if(Dbg_IsRunning()){
        DEBUG_MSG("Already running\n");
        return;
    }

    AppSetDelayedCall(DbgApp_DelayedSave);

    dbgSync.inQ.clear();
    dbgSync.dbgLB = nullptr;
    dbgSync.is_stopped = false;
    dbgSync.targetView = AppGetActiveView();

    DbgApp_ClearLinebufferList();

    // register async callbacks
    Dbg_RegisterStopPoint(DbgApp_AsyncHandleStopPoint);
    Dbg_RegisterExit(DbgApp_AsyncHandleExit);
    Dbg_RegisterReportState(DbgApp_AsyncHandleState);
    Dbg_RegisterBreakpointFeedback(DbgApp_AsyncHandleBkptFeedback);
    Dbg_RegisterExpressionFeedback(DbgApp_AsyncHandleExpFeedback);
    // ...

    // inform the rendering API that we will start an event handling
    // that requires live update
    Graphics_AddEventHandler(DBG_POOL_INTERVAL, DbgApp_PoolMessages);

    // start the debugger on the given binary, this will launch a thread
    // and put the debugger on its main loop for in/out
    Dbg_Start(binaryPath, args);
}
