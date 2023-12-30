#include <timer.h>
#include <time.h>
#include <Windows.h>
/*
* Timer
*/
typedef struct{
    int monotonic;
    uint64_t frequency;
    uint64_t offset;
}Timer;

static Timer systemTimer;

uint64_t GetTimerValue(){
    uint64_t value;
    QueryPerformanceCounter((LARGE_INTEGER*) &value);
    return value;
}

void InitializeSystemTimer(){
    QueryPerformanceFrequency((LARGE_INTEGER*)&systemTimer.frequency);
    systemTimer.offset = GetTimerValue();
}

uint64_t GetTimerFrequency(){
    return systemTimer.frequency;
}

double GetElapsedTime(){
    return (double)(GetTimerValue() - systemTimer.offset) / GetTimerFrequency();
}

