/* date = December 21st 2021 21:35 */
#pragma once
#include <parallel.h>
#include <unordered_map>
#include <utilities.h>
#include <vector>

#define DBG_CHECK(dbg) do{if(!dbg) return false; if(!dbg->priv) return false; }while(0)
#define DBG_TREE_EXP_STR "{...}"


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
    DbgPackage(DbgCode c) : data(std::string()), idata(-1), code(c){}
    DbgPackage(std::string _file, int _line) : data(_file), idata(_line),
        code(DbgCode::Breakpoint){}
    DbgPackage(std::string d, DbgCode c) : data(d), code(c){}
    DbgPackage(std::string d, int n, DbgCode c) : data(d), idata(n), code(c){}

    std::string data;
    int idata;
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
* Evaluating expressions returns a tree on GDB and god knows what in msvc,
* so we need to clearly define it so multiple debuggers have a common interface.
*/
struct ExpressionPair{
    std::string owner;
    std::string value;
};

struct ExpressionTreeNode{
    ExpressionPair values;
    std::vector<struct ExpressionTreeNode *> nodes;
};

struct ExpressionTree{
    ExpressionTreeNode *root;
    std::string expression;
};

/*
* Groups stuff to return request information.
*/
struct BreakpointFeedback{
    DbgBkpt bkpt;
    bool rv;
};

struct ExpressionFeedback{
    std::string expression;
    std::string value;
    uint handle;
    bool rv;
};

struct Dbg;

/* user calls */
#define DBG_USER_STOP_POINT(name) void name(DbgStop *stop)
#define DBG_USER_EXIT(name) void name()
#define DBG_USER_REPORT_STATE(name) void name(DbgState state)
#define DBG_USER_BKPT_FEEDBACK(name) void name(BreakpointFeedback bkptFed)
#define DBG_USER_EXP_FEEDBACK(name) void name(ExpressionFeedback expFed)

typedef DBG_USER_STOP_POINT(DbgUserStopPoint);
typedef DBG_USER_EXIT(DbgUserExit);
typedef DBG_USER_REPORT_STATE(DbgUserReportState);
typedef DBG_USER_BKPT_FEEDBACK(DbgUserBkptFeedback);
typedef DBG_USER_EXP_FEEDBACK(DbgUserExpressionFeedback);

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
#define DBG_EVAL_PARSER(name) bool name(char *data, ExpressionTree &tree)

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
typedef DBG_EVAL_PARSER(DbgPlatformExpressionParser);

struct Dbg{
    /* User calls */
    DbgUserStopPoint *fnUser_stopPoint;
    DbgUserExit *fnUser_exit;
    DbgUserReportState *fnUser_reportState;
    DbgUserBkptFeedback *fnUser_bkptFeedback;
    DbgUserExpressionFeedback *fnUser_expFeedback;
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
    /*
    * This routine must be non-thread dependent, it must be able to solve parsing
    * without blocking and locally so that any part of cody can call this.
    */
    DbgPlatformExpressionParser *fn_evalParser;
    /* Queue for sending requests to debugger */
    ConcurrentTimedQueue<DbgPackage> packageQ;
    /*
    * Map of breakpoints, not really required but usefull so that other parts
    * of cody don't need to map what is currently the debugger breakpoint states.
    * Instead they can simply query the debugger for the breakpoints enabled/disabled
    * at any moment, this only have the downside of needing a lock but querying
    * shouldn't run at the same time as breakpoint modification routines so it
    * won't be an issue.
    */
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
void Dbg_RegisterExpressionFeedback(DbgUserExpressionFeedback *fn);

/*
* Parses a previously obtained expression evaluation into a expression tree.
*/
bool Dbg_ParseEvaluation(char *eval, ExpressionTree &tree);

/*
* Release the resources taken from a expression tree.
*/
void Dbg_FreeExpressionTree(ExpressionTree &tree);

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

