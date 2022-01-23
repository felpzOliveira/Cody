#include <mi_gdb.h>
#include <stdio.h>
#include <types.h>
#include <dbg.h>
#include <chrono>

#define MODULE_NAME "DBG-LINUX"

void cb_console(const char *str, void *data)
{
    //printf("CONSOLE> %s\n",str);
}

/* Note that unlike what's documented in gdb docs it isn't usable. */
void cb_target(const char *str, void *data)
{
    //printf("TARGET> %s\n",str);
}

void cb_log(const char *str, void *data)
{
    //printf("LOG> %s\n",str);
}

void cb_to(const char *str, void *data)
{
    //printf(">> %s",str);
}

void cb_from(const char *str, void *data)
{
    //printf("<< %s\n",str);
}

void cb_async(mi_output *o, void *data)
{

}

void fill_stop_reason(mi_stop *sr, DbgStop *stop){
    switch(sr->reason){
        case sr_bkpt_hit:{
            stop->reason = Bkpto_Stop;
            stop->bkpno = sr->bkptno;
        } break;
        case sr_function_finished:{
            stop->reason = FunctionFinshed_Stop;
        } break;
        case sr_end_stepping_range:{
            stop->reason = Next_Stop;
        } break;
        case sr_exited_signalled:
        case sr_exited:
        case sr_exited_normally: {
            stop->reason = Exit_Stop;
        } break;
        case sr_signal_received: {
            stop->reason = Signal_Stop;
        } break;
        default:{
            printf("[DBG] Stop reason not implemented ( %d ) - %s\n",
                   (int)sr->reason, mi_reason_enum_to_str(sr->reason));
        }
    }

    mi_frames *frame = sr->frame;
    if(sr->frame && frame->file && frame->func){
        stop->file = std::string(frame->file);
        stop->line = frame->line;
        stop->func = std::string(frame->func);
        stop->level = 0;
    }
}

int wait_for_stop(mi_h *h, uint timeout_ms, DbgStop *stop){
    int res = 1;
    mi_stop *sr;
    auto start = std::chrono::system_clock::now();

    while(!mi_get_response(h)){
        usleep(1000);
        auto end = std::chrono::system_clock::now();
        auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if(elapsed >= timeout_ms){
            return 0;
        }
    }

    /* The end of the async. */
    sr = mi_res_stop(h);
    stop->reason = No_Stop;
    if(sr){
        fill_stop_reason(sr, stop);
        mi_free_stop(sr);
    }else{
        DEBUG_MSG("Error while waiting\n");
        DEBUG_MSG("mi_error: %d\nmi_error_from_gdb: %s\n", mi_error, mi_error_from_gdb);
        res = 0;
    }
    return res;
}

struct DbgLinux{
    mi_h *h;
    mi_aux_term *child_vt;
};

bool Dbg_LinuxStartWith(Dbg *dbg, const char *binPath, const char *args){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    dbgLinux->h = mi_connect_local();
    if(!dbgLinux->h){
        printf("[DBG] Failed to connect to gdb\n");
        return false;
    }

    mi_set_console_cb(dbgLinux->h, cb_console, (void *)dbg);
    mi_set_target_cb(dbgLinux->h, cb_target, (void *)dbg);
    mi_set_log_cb(dbgLinux->h, cb_log, (void *)dbg);
    mi_set_async_cb(dbgLinux->h, cb_async, (void *)dbg);
    mi_set_to_gdb_cb(dbgLinux->h, cb_to, (void *)dbg);
    mi_set_from_gdb_cb(dbgLinux->h, cb_from, (void *)dbg);

    // I hate this, I hate it hate it hate it with a burning hate
    // I don't want to send data to tty, give me my data!
    dbgLinux->child_vt = gmi_look_for_free_vt();

    if(!gmi_target_terminal(dbgLinux->h, dbgLinux->child_vt ?
                            dbgLinux->child_vt->tty : ttyname(STDIN_FILENO)))
    {
        printf("[DBG] Failed to set create term: %s %s\n", binPath, args);
        mi_disconnect(dbgLinux->h);
        return false;
    }

    if(!gmi_set_exec(dbgLinux->h, binPath, args)){
        printf("[DBG] Failed to set exec: %s %s\n", binPath, args);
        mi_disconnect(dbgLinux->h);
        gmi_end_aux_term(dbgLinux->child_vt);
        return false;
    }

    return true;
}

bool Dbg_LinuxEvalExpression(Dbg *dbg, char *expression, char **out){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;
    if(!out) return false;

    *out = gmi_data_evaluate_expression(dbgLinux->h, expression);
    return *out != nullptr;
}

bool Dbg_LinuxSetBreakpoint(Dbg *dbg, const char *file, int line, int *bkpno){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    mi_bkpt *bk = gmi_break_insert(dbgLinux->h, file, line);
    if(!bk){
        printf("[DBG] Failed to set breakpoint\n");
        return false;
    }

    DEBUG_MSG("Breakpoint %d @ function: %s\n", bk->number, bk->func);
    *bkpno = bk->number;
    mi_free_bkpt(bk);
    return true;
}

bool Dbg_LinuxContinue(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_continue(dbgLinux->h)){
        printf("[DBG] Failed to continue binary\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxFinish(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_finish(dbgLinux->h)){
        printf("[DBG] Failed to finish function\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxInterrupt(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_interrupt(dbgLinux->h)){
        printf("[DBG] Failed to interrupt\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxRun(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_run(dbgLinux->h)){
        printf("[DBG] Failed to run binary\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxWaitEvent(Dbg *dbg, uint timeout_ms, DbgStop *stop, char *baseFolder){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!wait_for_stop(dbgLinux->h, timeout_ms, stop)){
        return false;
    }

    // adjust the stop reason based on the viewing stack
    mi_frames *stack = gmi_stack_list_frames(dbgLinux->h);
    mi_frames *ptr = stack;
    while(ptr != nullptr){
        if(ptr->file){
            uint n = strlen(ptr->file);
            std::string file = ExpandFilePath(ptr->file, n, baseFolder);
            if(FileExists((char *)file.c_str())){
                stop->level = ptr->level;
                stop->file = file;
                stop->line = ptr->line;
                stop->func = std::string(ptr->func);
                DEBUG_MSG("Got stack at %d - %s\n", ptr->level, ptr->file);
                break;
            }
        }
        ptr = ptr->next;
    }

    mi_free_frames(stack);

    return true;
}

bool Dbg_LinuxStep(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_step(dbgLinux->h)){
        printf("[DBG] Failed to perform step\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxNext(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    if(!gmi_exec_next(dbgLinux->h)){
        printf("[DBG] Failed to perform next\n");
        return false;
    }

    return true;
}

bool Dbg_LinuxTerminate(Dbg *dbg){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;

    gmi_gdb_exit(dbgLinux->h);
    mi_disconnect(dbgLinux->h);
    gmi_end_aux_term(dbgLinux->child_vt);

    return true;
}

bool Dbg_LinuxEnableBreakpoint(Dbg *dbg, int bkpno, bool enable){
    DBG_CHECK(dbg);
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    if(!dbgLinux->h) return false;
    return gmi_break_state(dbgLinux->h, bkpno, enable);
}

void Dbg_GetFunctions(Dbg *dbg){
    dbg->fn_startWith = Dbg_LinuxStartWith;
    dbg->fn_setBkpt = Dbg_LinuxSetBreakpoint;
    dbg->fn_step = Dbg_LinuxStep;
    dbg->fn_next = Dbg_LinuxNext;
    dbg->fn_run = Dbg_LinuxRun;
    dbg->fn_waitEvent = Dbg_LinuxWaitEvent;
    dbg->fn_continue = Dbg_LinuxContinue;
    dbg->fn_terminate = Dbg_LinuxTerminate;
    dbg->fn_enableBkpt = Dbg_LinuxEnableBreakpoint;
    dbg->fn_interrupt = Dbg_LinuxInterrupt;
    dbg->fn_finish = Dbg_LinuxFinish;
    dbg->fn_evalExpression = Dbg_LinuxEvalExpression;

    if(dbg->priv == nullptr){
        dbg->priv = new DbgLinux;
    }
}

void Dbg_Free(Dbg *dbg){
    DbgLinux *dbgLinux = (DbgLinux *)dbg->priv;
    delete dbgLinux;
}

