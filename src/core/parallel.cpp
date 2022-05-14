#include <parallel.h>
#include <stdio.h>
#include <stdlib.h>
#include <symbol.h>
#include <file_provider.h>
#include <sstream>

#define CMD_EXIT "__internal_exit__"

struct BuildParseCtx{
    std::string currMessage;
    std::string lastMessage;
    int lastState;
    int pstate;
    int depends;
    std::string lastFile;
};

struct BuildErrorInformation{
    std::vector<BuildError> errors;
    int visited;
    std::mutex mutex;
};

ConcurrentQueue<std::string> commandQ;
LockedLineBuffer lockedBuffer;
BuildErrorInformation buildErrors;

void ClearBuildErrors(){
    std::unique_lock<std::mutex> guard(buildErrors.mutex);
    buildErrors.errors.clear();
    buildErrors.visited = -1;
}

std::optional<BuildError> NextBuildError(){
    std::optional<BuildError> err = {};
    if(buildErrors.errors.size() > 0){
        if(buildErrors.visited < 0){
            buildErrors.visited = 0;
        }else{
            buildErrors.visited = (buildErrors.visited+1) % buildErrors.errors.size();
        }

        err = std::optional<BuildError>(buildErrors.errors[buildErrors.visited]);
    }
    return err;
}

std::optional<BuildError> PreviousBuildError(){
    std::optional<BuildError> err = {};
    if(buildErrors.errors.size() > 0){
        if(buildErrors.visited > 0){
            buildErrors.visited -= 1;
        }else{
            buildErrors.visited = buildErrors.errors.size();
        }
        printf("Looking for %d\n", buildErrors.visited);
        err = std::optional<BuildError>(buildErrors.errors[buildErrors.visited]);
    }
    return err;
}

static void BuildBufferSoftClear(int is_build){
    std::unique_lock<std::mutex> guard(lockedBuffer.mutex);
    LineBuffer_Free(lockedBuffer.lineBuffer);
    LineBuffer_AllocateInternal(lockedBuffer.lineBuffer);
    if(is_build){
        ClearBuildErrors();
    }
}

template<typename Fn> int ExecuteCommand(std::string cmd, const Fn &callback){
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
            callback(StringReplace(std::string(buffer), "\\n", "\n"), 0);
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

void FetchBuildErrors(std::vector<BuildError> &errors){
    std::unique_lock<std::mutex> guard(buildErrors.mutex);
    for(BuildError &err : buildErrors.errors){
        errors.push_back(err);
    }
}

void PushBuildErrors(BuildError *err){
    std::unique_lock<std::mutex> guard(buildErrors.mutex);
    bool insert = true;
    for(BuildError &e : buildErrors.errors){
        if(e.file == err->file && e.line == err->line){
            insert = false;
            break;
        }
    }

    if(insert){
        buildErrors.errors.push_back(*err);
#if defined(DEBUG_BUILD)
        printf("%s:%d => %s\n", err->file.c_str(), err->line, err->message.c_str());
#endif
    }
}

static bool IsBuildCommand(std::string cmd){
    bool is_build = false;
    std::vector<std::string> splitted;
    StringSplit(cmd, splitted);
    // TODO: Add as needed
    for(std::string &str : splitted){
        if(str == "make"){
            is_build = true;
        }
    }
    return is_build;
}

// TODO: The make parsing stuff is very adhoc we might want to think
//       about some better parsing as well as removing this much string copies.

void print_error(std::string &msg, BuildParseCtx &parseCtx, int is_err){
    std::string file, line;
    int rs = 0;
    int loc = 0;
    for(uint i = 0; i < msg.size(); i++){
        if(rs == 0){
            if(msg[i] == ':' || msg[i] == '('){
                rs = 1;
                if(i > 0){
                    file = StringTrim(msg.substr(0, i));
                    if(file.size() == 0){
                        file = parseCtx.lastFile;
                    }
                }else{
                    file = parseCtx.lastFile;
                }
            }
        }else if(rs == 1){
            char s = msg[i] - '0';
            if(msg[i] == ':' || msg[i] == ')'){
                rs = 2;
                loc = i+1;
                break;
            }

            if(!(s >= 0 && s <= 9)){
                rs = -1;
                break;
            }
            line.push_back(msg[i]);
        }
    }

    if(rs == 2){
        std::string content;
        if(parseCtx.depends){
            content = parseCtx.lastMessage;
            parseCtx.depends = 0;
        }else{
            content = msg.substr(loc);
        }
        int nret = content.size();
        for(uint i = 0; i < content.size(); i++){
            if(content[i] == '\n'){
                nret = i;
            }else if(content[i] != ' '){
                nret = content.size();
            }
        }

        uint lu = StringToUnsigned((char *)line.c_str(), line.size());
        BuildError error = {
            .file = file,
            .line = (int)lu,
            .is_error = is_err,
            .message = content.substr(0, nret),
        };
        parseCtx.lastFile = file;
        parseCtx.lastMessage = error.message;
        PushBuildErrors(&error);
    }else{
        printf("Could not parse error\n");
    }
}

void print_state(std::string &msg, BuildParseCtx &parseCtx){
    if(msg.size() > 0){
        if(parseCtx.pstate == 1){
            print_error(msg, parseCtx, 1);
        }else if(parseCtx.pstate == 2){
            print_error(msg, parseCtx, 0);
        }
        msg = std::string();
    }
}

// TODO: This routine is really terrible, re-do!
void build_process_line(std::string line, BuildParseCtx &parseCtx){
    if(parseCtx.pstate == 0){
        if(line.find(": error:") != std::string::npos){
            parseCtx.currMessage += line;
            parseCtx.pstate = 1;
        }else if(line.find(": warning:") != std::string::npos){
            parseCtx.currMessage += line;
            parseCtx.pstate = 2;
        }
    }else{
        if(line.find(": error:") != std::string::npos){
            print_state(parseCtx.currMessage, parseCtx);

            parseCtx.currMessage = line;
            parseCtx.lastState = parseCtx.pstate;
            parseCtx.pstate = 1;
        }else if(line.find(": warning:") != std::string::npos){
            print_state(parseCtx.currMessage, parseCtx);

            parseCtx.currMessage = line;
            parseCtx.lastState = parseCtx.pstate;
            parseCtx.pstate = 2;
        }else if(line.find("detected in the compilation") != std::string::npos){
            print_state(parseCtx.currMessage, parseCtx);
            parseCtx.lastState = parseCtx.pstate;
            parseCtx.pstate = 0;
        }else if(line.find("make") == 0){
            print_state(parseCtx.currMessage, parseCtx);
            parseCtx.lastState = parseCtx.pstate;
            parseCtx.pstate = 0;
        }else if(line[0] == '('){
            if(StringStartsWithInteger(line.substr(1))){
                print_state(parseCtx.currMessage, parseCtx);
                parseCtx.currMessage = line;
                parseCtx.depends = 1;
            }else{
                parseCtx.currMessage += "\n" + line;
            }
        }else if(line.find(": here") != std::string::npos){
            print_state(parseCtx.currMessage, parseCtx);
            parseCtx.currMessage = line;
            parseCtx.depends = 1;
        }
        else if(line.size() > 0){
            parseCtx.currMessage += "\n" + line;
        }else{
            print_state(parseCtx.currMessage, parseCtx);
        }
    }
}

static int state = 1;
void ExecutorMainLoop(){
    while(1){
        std::string cmd = commandQ.pop();
        if(cmd.size() > 0){
            if(cmd == std::string(CMD_EXIT)) break;
            std::string runningLine;
            BuildParseCtx parseCtx = {
                .currMessage = std::string(),
                .lastState = 0,
                .pstate = 0,
                .depends = 0,
                .lastFile = std::string()
            };

            bool is_build = IsBuildCommand(cmd);

            BuildBufferSoftClear(is_build);

            ExecuteCommand(cmd, [&](std::string msg, int done) -> void{
                LineBuffer *lineBuffer = lockedBuffer.lineBuffer;
                char *text = (char *)msg.c_str();
                uint size = msg.size();
                uint start = lineBuffer->lineCount-1;
                if(is_build){
                    uint where = 0;
                    for(uint i = 0; i < msg.size(); i++){
                        if(msg[i] == '\n'){
                            int d = i - where;
                            if(d > 0){
                                runningLine += msg.substr(where, i);
                                where = i+1;
                            }else{
                                where = msg.size();
                            }
                            build_process_line(runningLine, parseCtx);
                            runningLine.clear();
                        }
                    }

                    if(where < msg.size()){
                        runningLine += msg.substr(where);
                    }
                }

                lockedBuffer.mutex.lock();
                uint n = LineBuffer_InsertRawText(lineBuffer, text, size);
                // the fast gen is in-place and does not require a tokenizer
                LineBuffer_FastTokenGen(lineBuffer, start, n);
                if(done){
                    lockedBuffer.render_state = 1;
                    if(is_build && runningLine.size() > 0){
                        build_process_line(runningLine, parseCtx);
                    }
                }else{
                    lockedBuffer.render_state = 0;
                }

                lockedBuffer.mutex.unlock();
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
    lockedBuffer.lineBuffer = LineBuffer_AllocateInternal(nullptr);
    lockedBuffer.render_state = -1;
    std::thread(ExecutorMainLoop).detach();
}
