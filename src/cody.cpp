#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <parallel.h>
#include <app.h>
#include <file_provider.h>
#include <cryptoutil.h>
#include <arg_parser.h>
#include <storage.h>
#include <security_services.h>
#include <modal.h>
#include <rng.h>

typedef struct{
    bool is_remote;
    bool use_tabs;
    std::string ip;
    int port;
    uint8_t key[32];
    bool has_key;
    std::string unknownPath;
}CmdLineArgs;

/* global options for the editor, can be changed based on command line tho */
void InitGlobals(){
    ENABLE_MODAL_MODE = true; /* change with --no-modal */
    LEX_DISABLE_PROC_STACK = false; /* change with --no-proc */
}

void DefaultArgs(CmdLineArgs *args){
    args->is_remote = false;
    args->use_tabs = false;
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

ARGUMENT_PROCESS(use_tabs_flags){
    CmdLineArgs *args = (CmdLineArgs *)config;
    args->use_tabs = true;
    return ARG_OK;
}

ARGUMENT_PROCESS(disable_proc_stack){
    LEX_DISABLE_PROC_STACK = true;
    return ARG_OK;
}

ARGUMENT_PROCESS(disable_dual_mode){
    ENABLE_MODAL_MODE = false;
    return ARG_OK;
}

ARGUMENT_PROCESS(set_encoding){
    std::string type = ParseNext(argc, argv, i, "--encoding");
    if(type.size() == 0){
        printf("Missing encoding type\n");
        return ARG_ABORT;
    }
    if(!SetGlobalDefaultEncoding(type)){
        return ARG_ABORT;
    }
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
    },
    {"--no-proc",
        { .processor = disable_proc_stack,
            .help = "Disable interpretation of complex syntax (struct/typedef/...)." }
    },
    {"--use-tabs",
        { .processor = use_tabs_flags,
          .help = "Sets the editor to use tabs instead of spaces for indentation." }
    },
    {"--no-modal",
        { .processor = disable_dual_mode,
          .help = "Disable modal mode for the editor." }
    },
    {"--encoding",
        { .processor = set_encoding,
          .help = "Sets the default encoding method." }
    }
};

void LoadStaticFilesOnStart(){
    // TODO: Port to storage arch
    std::string path = AppGetConfigFilePath();
    FILE *fp = fopen(path.c_str(), "r");
    if(fp){
        char line[256];
        char folder[PATH_MAX];
        FileEntry entry;
        std::string rootPath = AppGetRootDirectory();
        while (fgets(line, sizeof(line), fp) != NULL){
            {
                uint n = (uint)strlen(line);
                line[n-1] = 0;

                std::string lineStr(line);
                SwapPathDelimiter(lineStr);

                std::string p = rootPath + std::string(SEPARATOR_STRING) + lineStr;

                if(AppIsStoredFile(p)) continue;

                int r = GuessFileEntry((char *)p.c_str(), (uint)p.size(), &entry, folder);
                int fileType = -1;
                if(!(r < 0) && entry.type == DescriptorFile){
                    if(FileProvider_Load((char *)p.c_str(), (uint)p.size(), fileType))
                        AppAddStoredFile(lineStr);
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
        FileProvider_CreateFile((char *)path, (uint)strlen(path), &lineBuffer, nullptr);
        BufferView_SwapBuffer(view, lineBuffer, CodeView);
    }

    Graphics_Initialize();
}

void StartWithFile(const char *path=nullptr){
    BufferView *bView = nullptr;
    InitializeEmptyView(&bView);

    if(path != nullptr){
        LineBuffer *lineBuffer = nullptr;
        uint length = (uint)strlen(path);
        int fileType = -1;

        if(FileProvider_Load((char *)path, length, fileType, &lineBuffer, false))
            BufferView_SwapBuffer(bView, lineBuffer, CodeView);
    }else{
        LoadStaticFilesOnStart();
    }

    Graphics_Initialize();
}

void DEVEL_MSG(){
    printf("\n* Cody - Built %s at %s *\n", __DATE__, __TIME__);
}

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <dr_mp3.h>

#include <audio.h>
int test_call(){
    InitializeAudioSystem();

    const char* path =
            "/home/felipe/Documents/TaichiExpl/Dominus_Tenebris.mp3";

    char arg[AUDIO_MESSAGE_MAX_SIZE];
    AudioMessage message;
    message.code = AUDIO_CODE_PLAY;
    int n = snprintf(arg, AUDIO_MESSAGE_MAX_SIZE, "%s", path);

    memcpy(message.argument, arg, n * sizeof(char));

    AudioRequest(message);

    while(1){}
}

int _test_call()
{
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if(!SDL_Init(SDL_INIT_AUDIO)){
        printf("SDL audio init failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* path = "/home/felipe/Documents/TaichiExpl/Dominus_Tenebris.mp3";

    drmp3 mp3;
    if(!drmp3_init_file(&mp3, path, nullptr)){
        printf("failed to open mp3\n");
        SDL_Quit();
        return 1;
    }

    const drmp3_uint64 frames = drmp3_get_pcm_frame_count(&mp3);
    std::vector<int16_t> pcm(frames * mp3.channels);
    const drmp3_uint64 read_frames =
        drmp3_read_pcm_frames_s16(&mp3, frames, pcm.data());
    drmp3_uninit(&mp3);

    if(read_frames == 0){
        printf("decoded 0 frames\n");
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec spec{};
    spec.freq     = (int)mp3.sampleRate;
    spec.format   = SDL_AUDIO_S16;
    spec.channels = (Uint8)mp3.channels;

    SDL_AudioStream* stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                  &spec, nullptr, nullptr);
    if(!stream){
        printf("SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_ResumeAudioStreamDevice(stream);

    const int bytes = (int)(read_frames * mp3.channels * sizeof(int16_t));
    if(SDL_PutAudioStreamData(stream, pcm.data(), bytes) < 0){
        printf("SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_Quit();
        return 1;
    }

    SDL_FlushAudioStream(stream);  // optional here

    // Just sleep for the length of the audio plus a tiny safety margin.
    const Uint32 ms = (Uint32)((1000.0 * read_frames) / spec.freq);
    SDL_Delay(ms + 200);  // 200ms pad

    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 0;
}

int cody_entry(int argc, char **argv){
    //test_call();
    //return 0;
    //DEVEL_MSG();
    SecurityServices::Context context;
    CmdLineArgs args;
    DefaultArgs(&args);
    InitGlobals();

    StorageDeviceEarlyInit();

    ArgumentProcess(arg_map, argc, argv, "Cody", (void *)&args,
    [&](std::string val) -> bool
    {
        // got too many unknowns
        if(args.unknownPath.size() > 0)
            return ARG_ABORT;
        // one unkown we handle as the path of a file or dir
        args.unknownPath = val;
        return ARG_OK;
    }, 0);

    Crypto_InitRNGEngine();

    if(args.has_key)
        SecurityServices::CreateContext(context, args.key);
    else
        SecurityServices::CreateContext(context);

    if(!args.is_remote){
        SetStorageDevice(StorageDeviceType::Local);
    }else{
        SetStorageDevice(StorageDeviceType::Remote, args.ip.c_str(),
                         args.port, &context);
    }

    DebuggerRoutines();

    CommandExecutorInit();

    if(!args.is_remote){
        if(args.unknownPath.size() > 0){
            char folder[PATH_MAX];
            char* p = (char*)args.unknownPath.c_str();
            uint len = (uint)strlen(p);
            FileEntry entry;
            int r = GuessFileEntry(p, len, &entry, folder);

            if(r == -1){
                AppEarlyInitialize(args.use_tabs);
                StartWithNewFile(p);
                return 0;
            }

            CODY_IGNORE(CHDIR(folder));
            AppEarlyInitialize(args.use_tabs);

            if(entry.type == DescriptorFile){
                StartWithFile(entry.path);
            }else{
                StartWithFile();
            }

        }else{
            AppEarlyInitialize(args.use_tabs);
            StartWithFile();
        }
    }else{
        AppEarlyInitialize(args.use_tabs);
        AppSetPathCompression(2);
        BufferView *bView = nullptr;
        InitializeEmptyView(&bView);
        Graphics_Initialize();
    }

    return 0;
}

// for debug builds we should probably compile this under console subsystem and use main instead
#if defined(_WIN32)
//#if 0
#include <Windows.h>
#include <ShlObj.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
    LPWSTR* lpArgv;
    int argc;
    int addBaseDir = 0;

    LPWSTR wideCmdLine = GetCommandLineW();
    lpArgv = CommandLineToArgvW(wideCmdLine, &argc);

    int totalArgc = argc;

    if(argc == 1){
        totalArgc += 1;
        addBaseDir = 1;
    }

    char** argv = new char* [totalArgc];
    for(int i = 0; i < argc; ++i){
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, lpArgv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = new char[size_needed];
        WideCharToMultiByte(CP_UTF8, 0, lpArgv[i], -1, argv[i], size_needed, NULL, NULL);
    }

    if(addBaseDir){
        PWSTR userProfile = nullptr;
        HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &userProfile);
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, userProfile, -1, NULL, 0, NULL, NULL);
        char* userProfileChar = new char[bufferSize];
        WideCharToMultiByte(CP_UTF8, 0, userProfile, -1, userProfileChar, bufferSize, NULL, NULL);

        argv[totalArgc - 1] = userProfileChar;
        CoTaskMemFree(userProfile);
    }

    int rv = cody_entry(totalArgc, argv);

    for(int i = 0; i < totalArgc; ++i){
        delete[] argv[i];
    }
    delete[] argv;
    LocalFree(lpArgv);
    return rv;
}
#else
int main(int argc, char** argv){
    return cody_entry(argc, argv);
}
#endif
