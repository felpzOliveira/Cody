/* date = October 31st 2023 21:44 */
#pragma once
#include <types.h>

/*
* Initializes the global timer.
*/
void InitializeSystemTimer();

/*
* Gets raw timer value.
*/
uint64_t GetTimerValue();

/*
* Gets raw timer frequency.
*/
uint64_t GetTimerFrequency();

/*
* Get elapsed time from initialization.
*/
double GetElapsedTime();

