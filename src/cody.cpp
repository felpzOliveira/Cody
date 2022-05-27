#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <parallel.h>
#include <app.h>
#include <file_provider.h>
#include <bignum.h>
#include <cryptoutil.h>
#include <mp.h>
#include <gitbase.h>
#include <arg_parser.h>
#include <storage.h>

typedef struct{
    bool is_remote;
    std::string ip;
    int port;
    std::string unknownPath;
}CmdLineArgs;

void DefaultArgs(CmdLineArgs *args){
    args->is_remote = false;
    args->ip = "127.0.0.1";
    args->port = 1000;
    args->unknownPath = std::string();
}

ARGUMENT_PROCESS(remote_flags){
    CmdLineArgs *args = (CmdLineArgs *)config;
    std::string ip = ParseNext(argc, argv, i, "--remote");
    int port       = (int)ParseNextFloat(argc, argv, i, "--remote");
    if(port <= 0 || port >= 65535){
        printf("Invalid port value (%d)\n", port);
        return ARG_ABORT;
    }
    args->is_remote = true;
    args->ip = ip;
    args->port = port;
    return ARG_OK;
}

std::map<const char *, ArgDesc> arg_map = {
    {"--remote",
        { .processor = remote_flags,
          .help = "Use cody as a remote editor for a running server." }
    },
};

void testMP(){
    MPI val32 = MPI::FromI64(32);
    MPI val64 = MPI::FromU64(922337203685477580);
    std::string v("C34FFFFFFFFFFFF3CB00");
    std::string v2("922337203685477580");
    Bignum b, b1, b2;
    Bignum_FromString(&b, v.c_str(), 16);
    Bignum_FromString(&b1, v2.c_str(), 10);

    MPI mpit = MPI::FromHex(v);
    MPI mp = val64 + mpit;

    Bignum_Add(&b2, &b1, &b);
    std::cout << mp.ToRadix(16) << std::endl;
#if 1
    char ss[4096];
    size_t olen = 4096;
    Bignum_ToString(&b2, ss, 4096, &olen, 16);

    std::cout << ss << std::endl;
#endif

    //exit(0);
}

void LoadStaticFilesOnStart(){
    std::string path = AppGetConfigFilePath();
    FILE *fp = fopen(path.c_str(), "r");
    if(fp){
        char *line = nullptr;
        size_t lineSize = 0;
        int read = -1;
        char folder[PATH_MAX];
        FileEntry entry;
        std::string rootPath = AppGetRootDirectory();
        while((read = getline(&line, &lineSize, fp)) != -1){
            if(read > 0){
                line[read-1] = 0; // remove \n
                std::string p = rootPath + std::string("/") + std::string(line);

                if(AppIsStoredFile(p)) continue;

                int r = GuessFileEntry((char *)p.c_str(), p.size(), &entry, folder);
                if(!(r < 0) && entry.type == DescriptorFile){
                    FileProvider_Load((char *)p.c_str(), p.size());
                    AppAddStoredFile(line);
                }
            }
        }
        fclose(fp);
    }
}

void InitializeEmptyView(BufferView **view=nullptr){
    BufferView *bView = AppGetActiveBufferView();
    BufferView_Initialize(bView, nullptr, EmptyView);
    if(view) *view = bView;
}

void StartWithNewFile(const char *path=nullptr){
    BufferView *view = nullptr;
    InitializeEmptyView(&view);
    if(path){
        LineBuffer *lineBuffer = nullptr;
        FileProvider_CreateFile((char *)path, strlen(path), &lineBuffer, nullptr);
        BufferView_SwapBuffer(view, lineBuffer, CodeView);
    }

    Graphics_Initialize();
}

void StartWithFile(const char *path=nullptr){
    BufferView *bView = nullptr;
    InitializeEmptyView(&bView);

    if(path != nullptr){
        LineBuffer *lineBuffer = nullptr;
        uint length = strlen(path);

        FileProvider_Load((char *)path, length, &lineBuffer, false);
        BufferView_SwapBuffer(bView, lineBuffer, CodeView);
    }else{
        LoadStaticFilesOnStart();
    }

    Graphics_Initialize();
}

#include <server.h>
int port = 2000;
void server_test(){
    RPCServer server;
    server.Start(port);
}

void client_test(){
    RPCClient client;
    client.ConnectTo("127.0.0.1", port);
}

int main(int argc, char **argv){
    //server_test();
    //client_test();
    //return 0;
#if 0
    Git_Initialize();

    Git_OpenDirectory("/home/felipe/Documents/Lit");
    double interval = MeasureInterval([&](){
        Git_LogGraph();
    });

    printf("Graph generation took: %g ms\n", interval * 1000.0);

    Git_Finalize();
    return 0;

    testMP();
    return 0;
#endif
    CmdLineArgs args;
    DefaultArgs(&args);

    ArgumentProcess(arg_map, argc, argv, "Cody", (void *)&args,
    [&](std::string val) -> bool
    {
        // got too many unknowns
        if(args.unknownPath.size() > 0) return ARG_ABORT;
        // one unkown we handle as the path of a file or dir
        args.unknownPath = val;
        return ARG_OK;
    }, 0);

    if(!args.is_remote){
        SetStorageDevice(StorageDeviceType::Local);
    }else{
        // TODO: Setup remote stuff
    }

    DebuggerRoutines();

    CommandExecutorInit();

    if(args.unknownPath.size() > 0){
        char folder[PATH_MAX];
        char *p = (char *)args.unknownPath.c_str();
        uint len = strlen(p);
        FileEntry entry;
        int r = GuessFileEntry(p, len, &entry, folder);

        if(r == -1){
            printf("Unknown argument, attempting to open as file\n");
            AppEarlyInitialize();
            //StartWithFile();
            StartWithNewFile(p);
            return 0;
        }

        IGNORE(CHDIR(folder));
        AppEarlyInitialize();

        if(entry.type == DescriptorFile){
            StartWithFile(entry.path);
        }else{
            StartWithFile();
        }

    }else{
        AppEarlyInitialize();
        StartWithFile();
    }

    return 0;
}
