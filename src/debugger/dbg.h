/* date = December 21st 2021 21:35 */
#pragma once
#include <parallel.h>
#include <unordered_map>

#define DBG_CHECK(dbg) do{if(!dbg) return false; if(!dbg->priv) return false; }while(0)

typedef enum{
    Continue=0, Breakpoint, Step, Run, Next, Exit, Finish, Interrupt, Evaluate
}DbgCode;

typedef enum{
    Bkpto_Stop, Wp_Stop, FunctionFinshed_Stop,
    Next_Stop, Exit_Stop, No_Stop, Signal_Stop
}DbgStopReason;

typedef enum{
    Running = 0, Ready, Break, Finished, Exited, Signaled
}DbgState;

inline const char *DbgStateToString(DbgState state){
#define STR(val) case val: return #val
    switch(state){
        STR(DbgState::Running);
        STR(DbgState::Ready);
        STR(DbgState::Break);
        STR(DbgState::Finished);
        STR(DbgState::Exited);
        STR(DbgState::Signaled);
        default:{
            return "DbgState::Unknown";
        }
    }
#undef STR
}

class DbgPackage{
    public:
    DbgPackage(){}
    DbgPackage(DbgCode c) : data(std::string()), line(-1), code(c){}
    DbgPackage(std::string _file, int _line) : data(_file), line(_line),
        code(DbgCode::Breakpoint){}
    DbgPackage(std::string d, DbgCode c) : data(d), code(c){}

    std::string data;
    int line;
    DbgCode code;
};

struct DbgBkpt{
    std::string file;
    int line;
    int bkpno;
    bool enabled;
};

struct DbgStop{
    DbgStopReason reason;
    int bkpno;
    std::string file;
    std::string func;
    int level;
    int line;
};

/*
* Groups stuff to return information about success of adding breakpoints.
*/
struct BreakpointFeedback{
    DbgBkpt bkpt;
    bool rv;
};

struct Dbg;

/* user calls */
#define DBG_USER_STOP_POINT(name) void name(DbgStop *stop)
#define DBG_USER_EXIT(name) void name()
#define DBG_USER_REPORT_STATE(name) void name(DbgState state)
#define DBG_USER_BKPT_FEEDBACK(name) void name(BreakpointFeedback bkptFed)

typedef DBG_USER_STOP_POINT(DbgUserStopPoint);
typedef DBG_USER_EXIT(DbgUserExit);
typedef DBG_USER_REPORT_STATE(DbgUserReportState);
typedef DBG_USER_BKPT_FEEDBACK(DbgUserBkptFeedback);

/* function pointers for platforms */
#define DBG_START_WITH(name) bool name(Dbg *dbg, const char *binp, const char *args)
#define DBG_SET_BKPT(name) bool name(Dbg *dbg, const char *file, int lin, int *bkpno)
#define DBG_ENABLE_BKPT(name) bool name(Dbg *dbg, int bkpno, bool enable)
#define DBG_STEP(name) bool name(Dbg *dbg)
#define DBG_NEXT(name) bool name(Dbg *dbg)
#define DBG_RUN(name) bool name(Dbg *dbg)
#define DBG_PAUSE(name) bool name(Dbg *dbg)
#define DBG_TERMINATE(name) bool name(Dbg *dbg)
#define DBG_CONTINUE(name) bool name(Dbg *dbg)
#define DBG_WAIT_EVENT(name) bool name(Dbg *dbg, uint ms, DbgStop *stop, char *folder)
#define DBG_INTERRUPT(name) bool name(Dbg *dbg)
#define DBG_FINISH(name) bool name(Dbg *dbg)
#define DBG_EVAL_EXPRESSION(name) bool name(Dbg *dbg, char *expression, char **out)

typedef DBG_START_WITH(DbgPlatformStartWith);
typedef DBG_SET_BKPT(DbgPlatformSetBreakpoint);
typedef DBG_ENABLE_BKPT(DbgPlatformEnableBreakpoint);
typedef DBG_STEP(DbgPlatformStep);
typedef DBG_NEXT(DbgPlatformNext);
typedef DBG_RUN(DbgPlatformRun);
typedef DBG_PAUSE(DbgPlatformPause);
typedef DBG_WAIT_EVENT(DbgPlatformWaitEvent);
typedef DBG_CONTINUE(DbgPlatformContinue);
typedef DBG_TERMINATE(DbgPlatformTerminate);
typedef DBG_INTERRUPT(DbgPlatformInterrupt);
typedef DBG_FINISH(DbgPlatformFinish);
typedef DBG_EVAL_EXPRESSION(DbgPlatformEvalExpression);

struct Dbg{
    /* User calls */
    DbgUserStopPoint *fnUser_stopPoint;
    DbgUserExit *fnUser_exit;
    DbgUserReportState *fnUser_reportState;
    DbgUserBkptFeedback *fnUser_bkptFeedback;
    /* OS-dependent calls for the actual debugger */
    DbgPlatformStartWith *fn_startWith;
    DbgPlatformSetBreakpoint *fn_setBkpt;
    DbgPlatformEnableBreakpoint *fn_enableBkpt;
    DbgPlatformStep *fn_step;
    DbgPlatformNext *fn_next;
    DbgPlatformRun *fn_run;
    DbgPlatformPause *fn_pause;
    DbgPlatformContinue *fn_continue;
    DbgPlatformWaitEvent *fn_waitEvent;
    DbgPlatformTerminate *fn_terminate;
    DbgPlatformInterrupt *fn_interrupt;
    DbgPlatformFinish *fn_finish;
    DbgPlatformEvalExpression *fn_evalExpression;
    /* Queue for sending requests to debugger */
    ConcurrentTimedQueue<DbgPackage> packageQ;
    /* Map of breakpoints, not really required but usefull */
    std::unordered_map<std::string, DbgBkpt> brkMap;
    /* Actual debugger data, OS-dependent */
    void *priv;
};

/*
* Note: This currently only works for C/C++.
*/

/*
* Configure the callback pointer for user functions.
*/
void Dbg_RegisterStopPoint(DbgUserStopPoint *fn);
void Dbg_RegisterExit(DbgUserExit *fn);
void Dbg_RegisterReportState(DbgUserReportState *fn);
void Dbg_RegisterBreakpointFeedback(DbgUserBkptFeedback *fn);

/*
* Load the debugger with a specific binary file to be debugged.
*/
bool Dbg_Start(const char *binaryPath, const char *args);

/*
* Retrieves a list of all breakpoints currently registered in the debugger.
*/
void Dbg_GetBreakpointList(std::vector<DbgBkpt> &bkpts);

/*
* Checks if the deubgger is running _at_this_moment_
*/
bool Dbg_IsRunning();

/*
* Send a request to the debugger.
*/
void Dbg_SendPackage(DbgPackage package);

