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
#include <security.h>

typedef struct{
    bool is_remote;
    std::string ip;
    int port;
    uint8_t key[32];
    bool has_key;
    std::string unknownPath;
}CmdLineArgs;

void DefaultArgs(CmdLineArgs *args){
    args->is_remote = false;
    args->has_key = false;
    args->ip = "127.0.0.1";
    args->port = 1000;
    args->unknownPath = std::string();
}

void SetArgsKey(CmdLineArgs *args, uint8_t *mem){
    memcpy(args->key, mem, 32);
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

ARGUMENT_PROCESS(keyfile_flags){
    printf("Called@\n");
    uint size = 0;
    CmdLineArgs *args = (CmdLineArgs *)config;
    std::string path = ParseNext(argc, argv, i, "--keyfile");
    char *mem = GetFileContents(path.c_str(), &size);
    if(size != 32){
        printf("Invalid key size, key must be 32 bytes long\n");
        return ARG_ABORT;
    }

    if(!mem){
        printf("Failed to read key\n");
        return ARG_ABORT;
    }

    SetArgsKey(args, (uint8_t *)mem);
    AllocatorFree(mem);

    args->has_key = true;
    return ARG_OK;
}

ARGUMENT_PROCESS(keyval_flags){
    std::vector<uint8_t> key;
    CmdLineArgs *args = (CmdLineArgs *)config;
    std::string path = ParseNext(argc, argv, i, "--key");
    if(!(path.size() % 2 == 0 && path.size() > 0)){
        printf("Invalid key, key must be a valid hex value\n");
        return ARG_ABORT;
    }

    CryptoUtil_BufferFromHex(key, (char *)path.c_str(), path.size());
    if(key.size() != 32){
        printf("Invalid key size, key must be 32 bytes long\n");
        return ARG_ABORT;
    }

    SetArgsKey(args, key.data());
    args->has_key = true;
    return ARG_OK;
}

std::map<const char *, ArgDesc> arg_map = {
    {"--remote",
        { .processor = remote_flags,
          .help = "Use cody as a remote editor for a running server." }
    },
    {"--keyfile",
        { .processor = keyfile_flags,
          .help = "Sets a custom key to be used for a 256 bit key given in a file." }
    },
    {"--key",
        { .processor = keyval_flags,
          .help = "Sets a custom key to be used for AES 256 bit given as a hex string." }
    }
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
    // TODO: Port to storage arch
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
void client_test(){
    int port = 20000;
#if 1
    SetStorageDevice(StorageDeviceType::Remote, "127.0.0.1", port);
    FileEntry *entries = nullptr;
    uint n = 0;
    uint size = 0;

    StorageDevice *storage = FetchStorageDevice();
    if(storage->ListFiles((char *)"/home/felipe/Documents", &entries, &n, &size) < 0){
        printf("Failed to list files\n");
    }else{
        for(uint i = 0; i < n; i++){
            printf("Path = %s ( %s )\n", entries[i].path,
                   entries[i].type == DescriptorDirectory ? "Directory" : "File");
        }
    }

    if(entries) AllocatorFree(entries);

    if(!storage->AppendTo("/home/felipe/Documents/append_test.txt",
                          "Some random text 1\n", 0)) return;

    if(!storage->AppendTo("/home/felipe/Documents/append_test.txt",
                          "Some random text 2", 1)) return;

    if(!storage->AppendTo("/home/felipe/Documents/append_test.txt",
                          "Some random text 3", 0)) return;

    if(!storage->AppendTo("/home/felipe/Documents/append_test.txt",
                          "\nSome random text 4", 1)) return;
#else
    RPCClient client;
    client.ConnectTo("127.0.0.1", port);
    std::vector<uint8_t> out;

    if(!client.ReadEntireFile(out, "/home/felipe/Documents/lorem_ipsum.txt")){
        printf("Failed to read file\n");
    }else if(out.size() > 0){
        char *ptr = (char *)out.data();
        printf("File ( %lu ):\n%s\n", out.size(), ptr);
    }else{
        printf("Empty file\n");
    }

    out.clear();
    if(!client.StreamWriteStart(out, "/home/felipe/Documents/test_file.txt")){
        printf("Failed to open file\n");
    }else{
        printf("Opened file\n");
    }

    out.clear();
    std::string val("Hello from client!");
    if(!client.StreamWriteUpdate(out, (uint8_t *)val.c_str(), val.size())){
        printf("Failed to write file\n");
    }else{
        printf("Updated file\n");
    }

    out.clear();
    if(!client.StreamWriteFinal(out)){
        printf("Failed to close file\n");
    }else{
        printf("Closed file\n");
    }

    out.clear();
    if(client.Chdir(out, "/home/felipe/Documents")){
        printf("Chdir ok\n");
    }else{
        printf("Chdir failed\n");
    }

    out.clear();
    if(client.GetPwd(out)){
        std::string data((char *)out.data(), out.size());
        printf("Got pwd %s\n", data.c_str());
    }else{
        printf("Failed pwd\n");
    }
#endif
}

int main(int argc, char **argv){
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

    StorageDeviceEarlyInit();

    ArgumentProcess(arg_map, argc, argv, "Cody", (void *)&args,
    [&](std::string val) -> bool
    {
        // got too many unknowns
        if(args.unknownPath.size() > 0) return ARG_ABORT;
        // one unkown we handle as the path of a file or dir
        args.unknownPath = val;
        return ARG_OK;
    }, 0);

    if(args.has_key)
        SecurityServices::Start(args.key);
    else
        SecurityServices::Start();

    if(!args.is_remote){
        SetStorageDevice(StorageDeviceType::Local);
    }else{
        SetStorageDevice(StorageDeviceType::Remote,
                         args.ip.c_str(), args.port);
    }

    //client_test();
    //return 0;

    DebuggerRoutines();

    CommandExecutorInit();

    if(!args.is_remote){
        if(args.unknownPath.size() > 0){
            char folder[PATH_MAX];
            char *p = (char *)args.unknownPath.c_str();
            uint len = strlen(p);
            FileEntry entry;
            int r = GuessFileEntry(p, len, &entry, folder);

            if(r == -1){
                printf("Unknown argument, attempting to open as file\n");
                AppEarlyInitialize();
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
    }else{
        AppEarlyInitialize();
        BufferView *bView = nullptr;
        InitializeEmptyView(&bView);
        Graphics_Initialize();
    }

    return 0;
}
