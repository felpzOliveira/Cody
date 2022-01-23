/* date = December 28th 2021 18:43 */
#pragma once
#include <dbg.h>

/*
* Note: Functions that are DbgApp_Async* are not executed in the rendering thread
* and should manipulate buffers with care.
*/
//#define DBG_POOL_INTERVAL 0.008333 // 1 / 120
#define DBG_POOL_INTERVAL 0.0333 // 1 / 30

// TO_THINK: If these things were std::function we could store lambdas and simplify
//           calling it. Maybe get rid of function pointers and just use std::function
//           instead.
#define DBG_REPORT_STATE_CB(name) void name(DbgState state, void *priv)
#define DBG_BKPT_FEEDBACK_CB(name) void name(BreakpointFeedback feedback, void *priv)
typedef DBG_REPORT_STATE_CB(DbgApp_UserStateReport);
typedef DBG_BKPT_FEEDBACK_CB(DbgApp_UserBkptFeedback);

/*
* Handles a debugger stop point. This updates the interface accordingly to
* the stop point event.
*/
void DbgApp_AsyncHandleStopPoint(DbgStop *sp);

/*
* Handles application exit during a debugging session, either by a finish, a crash
* or a user requested exit.
*/
void DbgApp_AsyncHandleExit(void);

/*
* Register a new callback to be called whenever the debugger change states.
*/
uint DbgApp_RegisterStateChangeCallback(DbgApp_UserStateReport *fn, void *priv);

/*
* Register a new callback for reporting the state of a breakpoint whenever
* DbgApp_Break is called.
*/
uint DbgApp_RegisterBreakpointFeedbackCallback(DbgApp_UserBkptFeedback *fn, void *priv);

/*
* Unregister a previously registered callback by its handle.
*/
void DbgApp_UnregisterCallbackByHandle(uint handle);

/*
* Perform setup of Dbg and start it with the given binary and arguments.
*/
void DbgApp_StartDebugger(const char *binaryPath, const char *args);

/*
* Make the dbg app interface bind it's keyboard handler.
*/
void DbgApp_TakeKeyboard();

/*
* Check if the debugger _at_this_moment_ is at a breakpoint.
*/
bool DbgApp_IsStopped();

/*
* Sets a breakpoint.
*/
void DbgApp_Break(const char *file, uint line);

/*
* Evaluates an expression on the debugger state.
*/
void DbgApp_Eval(char *expression);

/*
* Sends the run command to the debugger.
*/
void DbgApp_Run();

/*
* Sends the continue command to the debugger.
*/
void DbgApp_Continue();

/*
* Sends the interrupt command to the debugger.
*/
void DbgApp_Interrupt();

/*
* Sends the function fininsh command to the debugger.
*/
void DbgApp_Finish();

/*
* Sends the step command to the debugger.
*/
void DbgApp_Step();

/*
* Sends the next command to the debugger.
*/
void DbgApp_Next();

/*
* Terminates the current session.
*/
void DbgApp_Exit();

/*
* Initialize internal data, i.e.: keyboard, ...
*/
void DbgApp_Initialize();
