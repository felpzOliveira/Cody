#include <server.h>
#include <storage.h>
#include <arg_parser.h>
#include <unistd.h>
#include <signal.h>
#include <security.h>
#include <cryptoutil.h>
#include <aes.h>

#define LOG_MODULE "RPC-Server"
#include <log.h>

typedef struct{
    int port;
    uint8_t key[32];
    bool has_key;
}CmdLineArgs;

void DefaultArgs(CmdLineArgs *args){
    args->port = 1000;
    args->has_key = false;
}

void SetArgsKey(CmdLineArgs *args, uint8_t *mem){
    memcpy(args->key, mem, 32);
}

ARGUMENT_PROCESS(remote_flags){
    CmdLineArgs *args = (CmdLineArgs *)config;
    int port       = (int)ParseNextFloat(argc, argv, i, "--remote");
    if(port <= 0 || port >= 65535){
        printf("Invalid port value (%d)\n", port);
        return ARG_ABORT;
    }
    args->port = port;
    return ARG_OK;
}

ARGUMENT_PROCESS(keyfile_flags){
    uint size = 0;
    CmdLineArgs *args = (CmdLineArgs *)config;
    std::string path = ParseNext(argc, argv, i, "--key");
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
    std::string path = ParseNext(argc, argv, i, "--keyval");
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

ARGUMENT_PROCESS(newkey_flags){
    uint8_t mem[32];
    CmdLineArgs *args = (CmdLineArgs *)config;
    if(!AES_GenerateKey(mem, AES256)){
        printf("Failed to generate a new key\n");
        return ARG_ABORT;
    }

    std::string value;
    CryptoUtil_BufferToHex(mem, 32, value);
    SetArgsKey(args, mem);
    unsigned int size = value.size();
    std::string title("* Started server with the key: ");

    size = size > (title.size()+1) ? size : (title.size()+1);
    printf(COLOR_YELLOW_BRIGHT);
    for(unsigned int i = 0; i < size; i++){
        printf("*");
    }

    printf("**\n%s", title.c_str());
    if(size > title.size()){
        for(unsigned int i = title.size(); i < size-1; i++){
            printf(" ");
        }
        printf("  *\n");
    }else{
        printf("\n");
    }
    printf(" %s \n*", value.c_str());
    for(unsigned int i = 0; i < size; i++){
        printf("*");
    }
    printf("*\n" COLOR_NONE);
    args->has_key = true;
    return ARG_OK;
}

std::map<const char *, ArgDesc> arg_map = {
    {"--port",
        { .processor = remote_flags,
          .help = "Sets the port to be used." }
    },
    {"--keyfile",
        { .processor = keyfile_flags,
          .help = "Sets a custom key to be used for AES 256 bit given in a file." }
    },
    {"--key",
        { .processor = keyval_flags,
          .help = "Sets a custom key to be used for AES 256 bit given as a hex string." }
    },
    {"--newkey",
        { .processor = newkey_flags,
          .help = "Start the server with a new key and dump it to the terminal." }
    }
};

RPCServer server;

void signal_handler(int s){
    server.Terminate();
    exit(0);
}

int main(int argc, char **argv){
    CmdLineArgs args;
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    DefaultArgs(&args);
    ArgumentProcess(arg_map, argc, argv, "Server", (void *)&args,
        [&](std::string val) -> bool
    {
        return ARG_ABORT;
    }, 0);

    StorageDeviceEarlyInit();
    SetStorageDevice(StorageDeviceType::Local);

    // TODO: Remove whenever we have key-exchange implemented
    if(args.has_key)
        SecurityServices::Start(args.key);
    else
        SecurityServices::Start();

    server.Start(args.port);
    return 0;
}
