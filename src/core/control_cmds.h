#pragma once
#include <keyboard.h>

/*
* Initializes the mappings for the control commands.
*/
void ControlCommands_Initialize();

/*
* Allows the control command interface to bind its mappings to a standard
* mapping.
*/
void ControlCommands_BindTrigger(BindingMap *mapping);

/*
* Forces the control command interface to yield control of
* the keyboard, restoring it to the last mapping activated
* before it took it.
*/
void ControlCommands_YieldKeyboard();

/*
* Restore the interface if it was in a expanded state.
*/
void ControlCommands_RestoreExpand();

/*
* Performs expansion/restoration of the view interface without relying
* on the control command being active.
*/
void ControlCommands_SwapExpand();

/*
* Queries to check if the view is currently expanded or not.
*/
int ControlCommands_IsExpanded();
