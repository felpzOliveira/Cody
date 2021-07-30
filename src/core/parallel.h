/* date = July 27th 2021 20:46 */
#pragma once
#include <thread>
#include <geometry.h>
#include <buffers.h>
#include <mutex>

struct LockedLineBuffer{
    LineBuffer *lineBuffer;
    std::mutex mutex;
    int render_state;
};

int ExecuteCommand(std::string cmd);
void CommandExecutorInit();
void FinishExecutor();
void GetExecutorLockedLineBuffer(LockedLineBuffer **ptr);

inline int GetConcurrency(){
    return Max(1, (int)std::thread::hardware_concurrency());
}

template<typename Function>
void ParallelFor(const char *desc, uint start, uint end, const Function &fn){
    if(start >= end) return;

    if(end - start == 1){
        for(uint j = start; j < end; j++){
            fn(j, 0);
        }
    }else{
        int it = 0;
        std::thread threads[128];
        int numThreads = GetConcurrency();

        uint n = end - start + 1;
        uint slice = (uint)std::round(n / (double)numThreads);
        slice = Max(slice, uint(1));

        auto helper = [&fn](uint j1, uint j2, int id){
            for(uint j = j1; j < j2; j++){
                fn(j, id);
            }
        };

        uint i0 = start;
        uint i1 = Min(start + slice, end);
        for(int i = 0; i + 1 < numThreads && i0 < end; i++){
            threads[it] = std::thread(helper, i0, i1, it);
            i0 = i1;
            i1 = Min(i1 + slice, end);
            it++;
        }

        if(i0 < end){
            threads[it] = std::thread(helper, i0, end, it);
            it++;
        }

        for(int i = 0; i < it; i++){
            if(threads[i].joinable()){
                threads[i].join();
            }
        }
    }
}
