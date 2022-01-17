/* date = December 28th 2021 18:43 */
#pragma once
#include <dbg.h>

/*
* Note: Functions that are DbgApp_Async* are not executed in the rendering thread
* and should manipulate buffers with care.
*/
//#define DBG_POOL_INTERVAL 0.008333 // 1 / 120
#define DBG_POOL_INTERVAL 0.0333 // 1 / 30

#define DBG_REPORT_STATE_CB(name) void name(DbgState state, void *priv)
typedef DBG_REPORT_STATE_CB(DbgApp_UserStateReport);

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
* Sets the callback to inform editor about debugger state change.
*/
void DbgApp_RegisterStateChangeCallback(DbgApp_UserStateReport *fn, void *priv);

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
* Sends the run command to the debugger.
*/
void DbgApp_Run();

/*
* Sends the function fininsh command to the debugger.
*/
void DbgApp_Finish();

/*
* Terminates the current session.
*/
void DbgApp_Exit();

/*
* Initialize internal data, i.e.: keyboard, ...
*/
void DbgApp_Initialize();
