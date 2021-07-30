#include <parallel.h>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <symbol.h>
#include <file_provider.h>

#define CMD_EXIT "__internal_exit__"

template<typename T> class ConcurrentQueue{
    public:

    std::mutex mutex;
    std::condition_variable cv;
    std::queue<T> itemQ;
    const unsigned int maxItems = 10;

    ConcurrentQueue() = default;
    T pop(){
        std::unique_lock<std::mutex> locker(mutex);
        while(itemQ.empty()){
            cv.wait(locker);
        }
        T val = itemQ.front();
        itemQ.pop();
        locker.unlock();

        cv.notify_one();
        return val;
    }

    void push(T item){
        std::unique_lock<std::mutex> locker(mutex);
        while(itemQ.size() >= maxItems){
            cv.wait(locker);
        }
        itemQ.push(item);
        locker.unlock();
        cv.notify_one();
    }
};

ConcurrentQueue<std::string> commandQ;
LockedLineBuffer lockedBuffer;

template<typename Fn>
int ExecuteCommand(std::string cmd, const Fn &callback){
    int rv = -1;
    char buffer[64];
    cmd += " 2>&1";
    FILE *fp = popen(cmd.c_str(), "r");
    if(!fp){
        return rv;
    }

    try{
        memset(buffer, 0, sizeof(buffer));
        while(fgets(buffer, sizeof(buffer), fp) != NULL){
            callback(std::string(buffer), 0);
        }

        callback(std::string(), 1);
        rv = 0;
    }catch(...){
        pclose(fp);
        fp = NULL;
    }

    if(fp){
        pclose(fp);
    }

    return rv;
}

static int state = 1;
void ExecutorMainLoop(){
    while(1){
        std::string cmd = commandQ.pop();
        if(cmd.size() > 0){
            if(cmd == std::string(CMD_EXIT)) break;
            LineBuffer_SoftClear(lockedBuffer.lineBuffer);
            ExecuteCommand(cmd, [&](std::string msg, int done) -> void{
                //printf("[LOOP]: %s", msg.c_str());
                LineBuffer *lineBuffer = lockedBuffer.lineBuffer;
                char *text = (char *)msg.c_str();
                uint size = msg.size();
                uint start = lineBuffer->lineCount-1;

                std::lock_guard<std::mutex> lock(lockedBuffer.mutex);
                uint n = LineBuffer_InsertRawText(lineBuffer, text, size);
                // the fast gen is in-place and does not require a tokenizer
                LineBuffer_FastTokenGen(lineBuffer, start, n);
                if(done){
                    lockedBuffer.render_state = 1;
                }else{
                    lockedBuffer.render_state = 0;
                }
            });
        }else{
            printf("Got empty cmd\n");
        }
    }

    state = 1;
}

int ExecuteCommand(std::string cmd){
    commandQ.push(cmd);
    return 0;
}

void FinishExecutor(){
    // cleanup of threaded queue is generating errors if we terminate before cleanup
    // so we are just going to sleep and wait for it
    state = 0;
    commandQ.push(CMD_EXIT);
    while(state == 0){
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void GetExecutorLockedLineBuffer(LockedLineBuffer **ptr){
    if(ptr){
        *ptr = &lockedBuffer;
    }
}

void CommandExecutorInit(){
    lockedBuffer.lineBuffer = AllocatorGetN(LineBuffer, 1);
    lockedBuffer.render_state = -1;
    LineBuffer_InitEmpty(lockedBuffer.lineBuffer);
    LineBuffer_SetStoragePath(lockedBuffer.lineBuffer, nullptr, 0);
    LineBuffer_SetWrittable(lockedBuffer.lineBuffer, false);
    LineBuffer_SetType(lockedBuffer.lineBuffer, 2); // 2 = Empty tokenizer
    LineBuffer_SetExtension(lockedBuffer.lineBuffer, FILE_EXTENSION_NONE);
    std::thread(ExecutorMainLoop).detach();
}
