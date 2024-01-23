/* date = July 27th 2021 20:46 */
#pragma once
#include <thread>
#include <geometry.h>
#include <buffers.h>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <typeindex>
#include <map>
#include <optional>
#include <chrono>
#include <iostream>

#define PARALLEL_QUEUE_TIMEOUT_MS 10

struct LockedLineBuffer{
    LineBuffer *lineBuffer;
    std::mutex mutex;
    int render_state;
};

struct BuildError{
    std::string file;
    int line;
    int is_error;
    std::string message;
};

///////////// Out of place routines ///////////////////
/*
* Returns the list of build errors for the last build command.
*/
void FetchBuildErrors(std::vector<BuildError> &errors);

/*
* Get the next build error.
*/
std::optional<BuildError> NextBuildError();

/*
* Get the previous build error.
*/
std::optional<BuildError> PreviousBuildError();


/*
* Clears the current build errors.
*/
void ClearBuildErrors();
///////////////////////////////////////////////////////

/*
* Utility routine for handling conditions variable under a function.
* Wait 'timeout_ms' under the function 'fn'. The function fn is directly passed
* to the 'wait_until' primitive so it must return a bool value that triggers
* the wait to stop. This routine itself returns < 0 in case the condition was
* not met and we timed out and > 0 when the condition was met.
*/
template<typename Fn>
inline int ConditionVariableWait(std::condition_variable &cv, std::mutex &mutex,
                                 long timeout_ms, Fn fn)
{
    std::unique_lock<std::mutex> locker(mutex);
    auto start = std::chrono::system_clock::now();
    auto max_poll_ms = std::chrono::milliseconds(timeout_ms);
    bool done = false;
    bool terminated = false;
    while(!done && !terminated){
        cv.wait_until(locker, start + max_poll_ms,
                      [&]{ terminated = fn(); return terminated; });

        auto end = std::chrono::system_clock::now();
        auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        done = elapsed >= timeout_ms;
    }

    return terminated ? 1 : -1;
}

/*
* Utility routine for handling condition variables under a function. Triggers one
* call to notify, possibly waking one thread.
*/
template<typename Fn>
inline void ConditionVariableTriggerOne(std::condition_variable &cv, std::mutex &mutex,
                                        Fn fn)
{
    std::unique_lock<std::mutex> locker(mutex);
    fn();
    locker.unlock();
    cv.notify_one();
}

/*
* Blocking Queue with timeout. Calling pop() blocks for 'max_timeout_ms'
* if an item is detected than it is returned otherwise an empty std::optional<T>
* is returned instead.
*/
template<typename T> class ConcurrentTimedQueue{
    public:
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<T> itemQ;
    uint max_timeout_ms;

    ConcurrentTimedQueue(uint ms=PARALLEL_QUEUE_TIMEOUT_MS) : max_timeout_ms(ms){}

    void set_interval(uint ms){ max_timeout_ms = ms; }

    std::optional<T> pop(){
        std::unique_lock<std::mutex> locker(mutex);
        auto max_ms = std::chrono::milliseconds(max_timeout_ms);
        bool done = false;
        auto start = std::chrono::system_clock::now();
        while(itemQ.empty() && !done){
            cv.wait_until(locker, start + max_ms, [&]{ return !itemQ.empty(); });
            auto end = std::chrono::system_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            done = elapsed >= max_timeout_ms;
        }

        if(itemQ.empty()){
            return {};
        }

        T val = itemQ.front();
        itemQ.pop();
        return std::optional<T>(val);
    }

    uint size(){
        std::unique_lock<std::mutex> locker(mutex);
        return itemQ.size();
    }

    void push(T item){
        std::unique_lock<std::mutex> locker(mutex);
        itemQ.push(item);
        locker.unlock();
        cv.notify_one();
    }

    void clear(){
        std::unique_lock<std::mutex> locker(mutex);
        std::queue<T> empty;
        std::swap(itemQ, empty);
    }
};

/*
* Blocking Queue. Calling pop() blocks untill a item is available.
*/
template<typename T> class ConcurrentQueue{
    public:

    std::mutex mutex;
    std::condition_variable cv;
    std::queue<T> itemQ;

    ConcurrentQueue() = default;
    T pop(){
        std::unique_lock<std::mutex> locker(mutex);
        while(itemQ.empty()){
            cv.wait(locker);
        }
        T val = itemQ.front();
        itemQ.pop();
        locker.unlock();

        return val;
    }

    void push(T item){
        std::unique_lock<std::mutex> locker(mutex);
        itemQ.push(item);
        locker.unlock();
        cv.notify_all();
    }

    int size(){
        std::unique_lock<std::mutex> locker(mutex);
        return itemQ.size();
    }

    void clear(){
        std::unique_lock<std::mutex> locker(mutex);
        std::queue<T> empty;
        std::swap(itemQ, empty);
    }
};

int ExecuteCommand(std::string cmd);
void CommandExecutorInit();
void FinishExecutor();
void GetExecutorLockedLineBuffer(LockedLineBuffer **ptr);

inline int GetConcurrency(){
    return (int)Max((Float)1, (Float)std::thread::hardware_concurrency());
}

/*
* NOTE: Whenever DispatchHost is called by the parallel routine
*       the host (who called DispatchExecution) is allowed to continue.
*       If the routine finishes, make sure the variables are still present
*       within the body of the function that was dispatched otherwise garbage
*       may be used instead creating some annoying bugs. For this you might
*       want to do something like:
*
*        void my_parallel_dispatcher(var_type1 var0, var_type2 var1){
*            DispatchExecution([&](HostDispatcher *dispatcher){
*                var_type1 local_var0 = var0;
*                var_type2 local_var1 = var1;
*                    ...
*               < can use both var0/1 and local_var0/1 here >
*                    ...
*                dispatcher->DispatchHost();
*                    ...
*               < using var0/1 here might cause a bug because their storage
*                 might have been requested again in the stack, use local_var0/1 instead >
*                    ...
*            });
*        }
*
*      some examples are: QueryBarHistory_DetachedLoad/Store and LineBuffer_Init.
*/
class HostDispatcher{
    public:
    std::mutex *mutex;
    std::condition_variable *cond;
    int dispatchFlag;

    HostDispatcher(){}

    int IsDispatched(){ return dispatchFlag; }

    void Init(std::mutex *m, std::condition_variable *c){
        mutex = m;
        cond = c;
        dispatchFlag = 0;
    }

    void DispatchHost(){
        std::lock_guard<std::mutex> guard(*mutex);
        dispatchFlag = 1;
        cond->notify_one();
    }
};

template<typename T> class Dispatcher{
    public:
    std::mutex mutex;
    std::condition_variable cond;
    HostDispatcher hostDisp;

    Dispatcher() = default;

    void DispatchFunction(const T &func){
        hostDisp.Init(&mutex, &cond);

        std::unique_lock<std::mutex> guard(mutex);
        std::thread(func, &hostDisp).detach();
        cond.wait(guard, [this]{ return hostDisp.dispatchFlag == 1; });
    }
};

template<typename Function>
void DispatchExecution(const Function &fn){
    static std::map<std::type_index, void *> dispatcherMap;
    std::type_index index = std::type_index(typeid(Function));
    Dispatcher<Function> *dispatcher = nullptr;

    if(dispatcherMap.find(index) == dispatcherMap.end()){
        dispatcher = new Dispatcher<Function>();
        dispatcherMap[index] = (void *)dispatcher;
    }else{
        dispatcher = (Dispatcher<Function> *)dispatcherMap[index];
    }

    dispatcher->DispatchFunction(fn);
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
