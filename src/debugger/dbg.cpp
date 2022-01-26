#include <dbg.h>
#include <sstream>
#include <iostream>
#include <queue>
#include <stack>

#define MODULE_NAME "DBG"

void Dbg_GetFunctions(Dbg *dbg);
void Dbg_Free(Dbg *dbg);

bool is_running = false;
std::mutex runMutex;
std::mutex bkptQueryMutex;
Dbg dbg = {
    .fnUser_stopPoint = nullptr,
    .fnUser_exit = nullptr,
    .fnUser_reportState = nullptr,
    .fnUser_bkptFeedback = nullptr,
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

static bool Dbg_HandleBreakpoint(Dbg *dbg, std::string file, int line, DbgBkpt *bkp){
    std::lock_guard<std::mutex> guard(bkptQueryMutex);
    bool rv = true;
    std::stringstream ss;
    ss << file << ":" << line;
    std::string key = ss.str();
    DbgBkpt brk;

    if(dbg->brkMap.find(key) != dbg->brkMap.end()){
        brk = dbg->brkMap[key];
        rv = dbg->fn_enableBkpt(dbg, brk.bkpno, !brk.enabled);
        if(rv){
            brk.enabled = !brk.enabled;
            dbg->brkMap[key] = brk;
        }
    }else{
        int bkpno = -1;
        rv = dbg->fn_setBkpt(dbg, file.c_str(), line, &bkpno);
        if(rv){
            brk.file = file;
            brk.line = line;
            brk.bkpno = bkpno;
            brk.enabled = true;
            dbg->brkMap[key] = brk;
        }
    }

    if(bkp){
        *bkp = brk;
    }

    return rv;
}

void Dbg_GetBreakpointList(std::vector<DbgBkpt> &bkpts){
    std::lock_guard<std::mutex> guard(bkptQueryMutex);
    bkpts.resize(dbg.brkMap.size());
    int counter = 0;
    for(auto it = dbg.brkMap.begin(); it != dbg.brkMap.end(); it++){
        bkpts[counter++] = it->second;
    }
}

static void Dbg_Cleanup(Dbg *dbg){
    dbg->brkMap.clear();
    Dbg_Free(dbg);
}

static void Dbg_PrintTree(ExpressionTree *tree){
    struct node_value{
        ExpressionTreeNode *node;
        int depth;
    };
    std::stack<node_value> nodeQ;
    ExpressionTreeNode *root = tree->root;
    nodeQ.push({ .node = root, .depth = 0, });
    int currDepth = 0;
    std::stack<std::string> depthOwner;
    std::string currOwner = root->owner;
    while(!nodeQ.empty()){
        node_value val = nodeQ.top();
        ExpressionTreeNode *node = val.node;
        int depth = val.depth;
        nodeQ.pop();

        if(node != root){
            if(currDepth > depth){
                printf("} ( %s ) ", currOwner.c_str());
                currOwner = std::string();
                if(!depthOwner.empty()){
                    currOwner = depthOwner.top();
                    depthOwner.pop();
                }else{
                    printf(" -- Finished ");
                }
            }else if(currDepth == depth){
                printf(", ");
            }
        }

        printf("%s = ", node->owner.c_str());
        if(node->value == DBG_TREE_EXP_STR){
            printf("{ ");
            depthOwner.push(currOwner);
            currOwner = node->owner;
        }else{
            printf("%s", node->value.c_str());
        }
        currDepth = depth;
        for(uint i = 0; i < node->nodes.size(); i++){
            nodeQ.push({ .node = node->nodes[i], .depth = depth+1 });
        }
        if(nodeQ.empty()){
            for(int i = 0; i < currDepth; i++){
                printf("} ( %s )", currOwner.c_str());
                currOwner = std::string();
                if(!depthOwner.empty()){
                    currOwner = depthOwner.top();
                    depthOwner.pop();
                }
            }
        }
    }
    printf("\n");
}

static void Dbg_PrintTree2(ExpressionTree *tree){
    struct node_value{
        ExpressionTreeNode *node;
        int depth;
    };

    std::queue<node_value> nodeQ;
    ExpressionTreeNode *root = tree->root;
    nodeQ.push({ .node = root, .depth = 0, });
    std::map<ExpressionTreeNode *,
            std::map<int, std::vector<ExpressionTreeNode *>>> nodeMap;
    while(!nodeQ.empty()){
        node_value val = nodeQ.front();
        ExpressionTreeNode *node = val.node;
        int depth = val.depth;

        nodeQ.pop();

        if(node != root){
            std::map<int, std::vector<ExpressionTreeNode *>> targetMap;
            std::vector<ExpressionTreeNode *> data;
            if(nodeMap.find(node) != nodeMap.end()){
                targetMap = nodeMap[node];
            }

            if(targetMap.find(depth) != targetMap.end()){
                data = targetMap[depth];
            }

            data.push_back(node);
            targetMap[depth] = data;
            nodeMap[node] = targetMap;
        }

        for(uint i = 0; i < node->nodes.size(); i++){
            nodeQ.push({ .node = node->nodes[i], .depth = depth+1 });
        }
    }

    printf("> %s = %s\n", root->owner.c_str(), root->value.c_str());
    for(auto it = nodeMap.begin(); it != nodeMap.end(); it++){
        std::map<int, std::vector<ExpressionTreeNode *>> targetMap = it->second;
        int n = 0;
        for(uint d = 0; d < targetMap.size(); ){
            if(targetMap.find(n) == targetMap.end()){
                n += 1;
            }else{
                std::vector<ExpressionTreeNode *> v = targetMap[n];
                for(ExpressionTreeNode *nd : v){
                    for(int i = 0; i < n; i++){
                        printf(" ");
                    }
                    printf("> %s = %s\n", nd->owner.c_str(), nd->value.c_str());
                }
                d++;
                n += 1;
            }
        }
    }
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
    if(dbg.fnUser_reportState){
        dbg.fnUser_reportState(DbgState::Ready);
    }

    while(main_loop_run){
        std::optional<DbgPackage> o_pkg = dbg.packageQ.pop();
        if(o_pkg){
            DbgPackage package = o_pkg.value();
            wait_event = 1;
            switch(package.code){
                case DbgCode::Run: {
                    rv = dbg.fn_run(&dbg);
                    if(rv && dbg.fnUser_reportState){
                        dbg.fnUser_reportState(DbgState::Running);
                    }
                } break;
                case DbgCode::Evaluate: {
                    char *out = nullptr;
                    rv = dbg.fn_evalExpression(&dbg, (char *)package.data.c_str(), &out);
                    if(out){
                        ExpressionTree tree = {
                            .root = nullptr,
                            .expression = package.data,
                        };
                        rv = Dbg_ParseEvaluation(out, tree);
                        free(out);
                        Dbg_PrintTree(&tree);
                    }
                    wait_event = 0;
                } break;
                case DbgCode::Continue: {
                    rv = dbg.fn_continue(&dbg);
                    if(rv && dbg.fnUser_reportState){
                        dbg.fnUser_reportState(DbgState::Running);
                    }
                } break;
                case DbgCode::Step: {
                    rv = dbg.fn_step(&dbg);
                    if(rv && dbg.fnUser_reportState){
                        dbg.fnUser_reportState(DbgState::Running);
                    }
                } break;
                case DbgCode::Next: {
                    rv = dbg.fn_next(&dbg);
                    if(rv && dbg.fnUser_reportState){
                        dbg.fnUser_reportState(DbgState::Running);
                    }
                } break;
                case DbgCode::Breakpoint: {
                    DbgBkpt bkpt;
                    rv = Dbg_HandleBreakpoint(&dbg, package.data, package.line, &bkpt);
                    if(dbg.fnUser_bkptFeedback){
                        BreakpointFeedback bkptFed = {
                            .bkpt = bkpt,
                            .rv = rv,
                        };
                        dbg.fnUser_bkptFeedback(bkptFed);
                    }
                    wait_event = 0;
                } break;
                case DbgCode::Finish: {
                    /*
                    * For finish to be more usefull we need to go back to a point
                    * where we can actually do something, it is possible that a pause
                    * got us into a very bad point so we need to go back all the way
                    * to a point where we can show code otherwise we might get some
                    * frustration.
                    */
                    bool done = false;
                    do{
                        rv = dbg.fn_finish(&dbg);
                        if(rv){
                            DbgStop stop;
                            rv = dbg.fn_waitEvent(&dbg, wait_timeout_ms, &stop, folder);
                            if(rv && stop.level == 0){
                                done = true;
                                if(stop.file.size() > 0){
                                    stop.file = ExpandFilePath((char *)stop.file.c_str(),
                                    stop.file.size(), folder);
                                    DEBUG_MSG("At %s : %s : %d [ %d ]\n", stop.file.c_str(),
                                               stop.func.c_str(), stop.line, stop.level);
                                }

                                if(dbg.fnUser_stopPoint) dbg.fnUser_stopPoint(&stop);

                                if(dbg.fnUser_reportState){
                                    if(stop.reason == Exit_Stop){
                                        dbg.fnUser_reportState(DbgState::Exited);
                                    }else if(stop.reason == Signal_Stop){
                                        dbg.fnUser_reportState(DbgState::Signaled);
                                    }else{
                                        dbg.fnUser_reportState(DbgState::Break);
                                    }
                                }
                            }
                        }else{
                            break;
                        }
                    }while(!done);
                    wait_event = 0;
                } break;
                case DbgCode::Interrupt: {
                    rv = dbg.fn_interrupt(&dbg);
                } break;
                case DbgCode::Exit: {
                    rv = dbg.fn_terminate(&dbg);
                    main_loop_run = false;
                    wait_event = 0;
                    if(rv && dbg.fnUser_reportState){
                        dbg.fnUser_reportState(DbgState::Exited);
                    }
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

                if(dbg.fnUser_reportState){
                    if(stop.reason == Exit_Stop){
                        dbg.fnUser_reportState(DbgState::Exited);
                    }else if(stop.reason == Signal_Stop){
                        dbg.fnUser_reportState(DbgState::Signaled);
                    }else{
                        dbg.fnUser_reportState(DbgState::Break);
                    }
                }
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
    dbg.brkMap.clear();
    std::thread(Dbg_Entry, e, a).detach();
    return true;
}

bool Dbg_ParseEvaluation(char *eval, ExpressionTree &tree){
    return dbg.fn_evalParser(eval, tree);
}

void Dbg_RegisterStopPoint(DbgUserStopPoint *fn){
    dbg.fnUser_stopPoint = fn;
}

void Dbg_RegisterExit(DbgUserExit *fn){
    dbg.fnUser_exit = fn;
}

void Dbg_RegisterReportState(DbgUserReportState *fn){
    dbg.fnUser_reportState = fn;
}

void Dbg_RegisterBreakpointFeedback(DbgUserBkptFeedback *fn){
    dbg.fnUser_bkptFeedback = fn;
}

