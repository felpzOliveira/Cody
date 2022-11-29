/* date = November 29th 2022 17:02 */
#pragma once

/*
* Initializes the dual mode inner keyboard binding.
*/
void DualModeInit();

/*
* Gets the current state of the modal state.
*/
int DualModeGetState();

/*
* Enters dual mode by locking write and setting the dual mode keyboard mapping
*/
void DualModeEnter();

/*
* Initialize the trigger and register the mapping for the dual mode.
*/
void DualMode_BindTrigger(BindingMap *mapping);
