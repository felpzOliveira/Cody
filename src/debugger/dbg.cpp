#include <dbg.h>
#include <sstream>
#include <iostream>

#define MODULE_NAME "DBG"

void Dbg_GetFunctions(Dbg *dbg);
void Dbg_Free(Dbg *dbg);

bool is_running = false;
std::mutex runMutex;
Dbg dbg = {
    .fnUser_stopPoint = nullptr,
    .fnUser_exit = nullptr,
};

static void Dbg_LockedSetRun(){
    std::unique_lock<std::mutex> locker(runMutex);
    is_running = true;
}

static void Dbg_LockedUnsetRun(){
    std::unique_lock<std::mutex> locker(runMutex);
    is_running = false;
}

bool Dbg_IsRunning(){
    std::unique_lock<std::mutex> locker(runMutex);
    return is_running;
}

void Dbg_SendPackage(DbgPackage package){
    dbg.packageQ.push(package);
}

static bool Dbg_HandleBreakpoint(Dbg *dbg, std::string file, int line){
    bool rv = true;
    std::stringstream ss;
    ss << file << ":" << line;
    std::string key = ss.str();

    if(dbg->brkMap.find(key) != dbg->brkMap.end()){
        DbgBkpt brk = dbg->brkMap[key];
        rv = dbg->fn_enableBkpt(dbg, brk.bkpno, !brk.enabled);
        if(rv){
            brk.enabled = !brk.enabled;
            dbg->brkMap[key] = brk;
        }
    }else{
        int bkpno = -1;
        rv = dbg->fn_setBkpt(dbg, file.c_str(), line, &bkpno);
        if(rv){
            DbgBkpt brk = {
                .file = file,
                .line = line,
                .bkpno = bkpno,
                .enabled = true,
            };
            dbg->brkMap[key] = brk;
        }
    }

    return rv;
}

static void Dbg_Cleanup(Dbg *dbg){
    dbg->brkMap.clear();
    Dbg_Free(dbg);
}

static void Dbg_Entry(std::string binaryPath, std::string args){
    bool main_loop_run = true;
    FileEntry entry;
    char folder[PATH_MAX];
    char *aptr = args.size() > 0 ? (char *)args.c_str() : nullptr;
    char *eptr = binaryPath.size() > 0 ? (char *)binaryPath.c_str() : nullptr;
    constexpr uint wait_timeout_ms = 20;
    bool rv = true;
    int wait_event = 0;

    Dbg_LockedSetRun();
    dbg.priv = nullptr;

    Dbg_GetFunctions(&dbg);

    int r = GuessFileEntry(eptr, binaryPath.size(), &entry, folder);
    if(r == -1){
        DEBUG_MSG("Cannot translate file %s\n", eptr);
        dbg.fn_terminate(&dbg);
        goto __terminate;
    }

    DEBUG_MSG("Attempting to load binary %s from %s\n", entry.path, folder);

    if(!dbg.fn_startWith(&dbg, eptr, aptr)){
        goto __terminate;
    }

    DEBUG_MSG("Successfully initialized\n");

    while(main_loop_run){
        std::optional<DbgPackage> o_pkg = dbg.packageQ.pop();
        if(o_pkg){
            DbgPackage package = o_pkg.value();
            wait_event = 1;
            switch(package.code){
                case DbgCode::Run: {
                    rv = dbg.fn_run(&dbg);
                } break;
                case DbgCode::Continue: {
                    rv = dbg.fn_continue(&dbg);
                } break;
                case DbgCode::Step: {
                    rv = dbg.fn_step(&dbg);
                } break;
                case DbgCode::Next: {
                    rv = dbg.fn_next(&dbg);
                } break;
                case DbgCode::Breakpoint: {
                    rv = Dbg_HandleBreakpoint(&dbg, package.file, package.line);
                    wait_event = 0;
                } break;
                case DbgCode::Finish: {
                    rv = dbg.fn_finish(&dbg);
                } break;
                case DbgCode::Interrupt: {
                    rv = dbg.fn_interrupt(&dbg);
                } break;
                case DbgCode::Exit: {
                    rv = dbg.fn_terminate(&dbg);
                    main_loop_run = false;
                    wait_event = 0;
                } break;
                default:{
                    printf("[DBG] Unknown package code\n");
                }
            }
        }

        if(wait_event && rv){
            DbgStop stop;
            if(dbg.fn_waitEvent(&dbg, wait_timeout_ms, &stop, folder)){
                // in some cases it is possible that dbg won't return
                // the full path for the stop point. This makes cody
                // life hard because it can't swap to the requested file,
                // lets expand the file path to make sure we at least try
                // to get the complete access to the file.
                // TODO: What happens if we fail here and return a
                //       path we can't follow?
                if(stop.file.size() > 0){
                    stop.file = ExpandFilePath((char *)stop.file.c_str(),
                                               stop.file.size(), folder);
                    DEBUG_MSG("At %s : %s : %d [ %d ]\n", stop.file.c_str(),
                              stop.func.c_str(), stop.line, stop.level);
                }

                if(dbg.fnUser_stopPoint)
                    dbg.fnUser_stopPoint(&stop);
            }
        }
    }

__terminate:
    DEBUG_MSG("Exiting dbg\n");
    if(dbg.fnUser_exit)
        dbg.fnUser_exit();

    Dbg_Cleanup(&dbg);
    Dbg_LockedUnsetRun();
}

bool Dbg_Start(const char *binaryPath, const char *args){
    std::unique_lock<std::mutex> locker(runMutex);
    std::string e = binaryPath ? std::string(binaryPath) : std::string();
    std::string a = args ? std::string(args) : std::string();
    if(is_running){
        printf("[DBG] Cannot start at this moment, already running\n");
        return false;
    }

    dbg.packageQ.clear();
    std::thread(Dbg_Entry, e, a).detach();
    return true;
}

void Dbg_RegisterStopPoint(DbgUserStopPoint *fn){
    dbg.fnUser_stopPoint = fn;
}

void Dbg_RegisterExit(DbgUserExit *fn){
    dbg.fnUser_exit = fn;
}

void dbg_entry(){
    Dbg_Start("/home/felipe/Documents/entry", nullptr);
    int run = 1;
    while(run){
        printf("dbg > ");
        fflush(stdout);
        std::string cmd;
        std::getline(std::cin, cmd);
        std::vector<std::string> vals;

        if(cmd.size() == 0){
            continue;
        }

        StringSplit(cmd, vals);
        DbgPackage package;
        if(vals[0] == "run"){
            package.code = DbgCode::Run;
        }else if(vals[0] == "continue"){
            package.code = DbgCode::Continue;
        }else if(vals[0] == "step"){
            package.code = DbgCode::Step;
        }else if(vals[0] == "next"){
            package.code = DbgCode::Next;
        }else if(vals[0] == "break"){
            package.code = DbgCode::Breakpoint;
            package.file = vals[1];
            package.line = std::stoi(vals[2]);
        }else if(vals[0] == "quit" || vals[0] == "exit"){
            package.code = DbgCode::Exit;
            run = 0;
        }
        else{
            printf("[DBG] Unknown command\n");
            continue;
        }

        Dbg_SendPackage(package);
    }
    while(is_running) ;
    printf("Terminated\n");
}
