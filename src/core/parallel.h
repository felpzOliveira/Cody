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

template<typename Index, typename Function>
void ParallelFor(const char *desc, Index start, Index end, const Function &fn){
    if(start >= end) return;

    if(end - start == 1){
        for(Index j = start; j < end; j++){
            fn(j);
        }
    }else{
        int it = 0;
        std::thread threads[128];
        int numThreads = GetConcurrency();

        Index n = end - start + 1;
        Index slice = (Index)std::round(n / (double)numThreads);
        slice = Max(slice, Index(1));

        auto helper = [&fn](Index j1, Index j2){
            for(Index j = j1; j < j2; j++){
                fn(j);
            }
        };

        Index i0 = start;
        Index i1 = Min(start + slice, end);
        for(int i = 0; i + 1 < numThreads && i0 < end; i++){
            threads[it++] = std::thread(helper, i0, i1);
            i0 = i1;
            i1 = Min(i1 + slice, end);
        }

        if(i0 < end){
            threads[it++] = std::thread(helper, i0, end);
        }

        for(int i = 0; i < it; i++){
            if(threads[i].joinable()){
                threads[i].join();
            }
        }
    }
}
