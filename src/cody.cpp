#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <unistd.h>
#include <app.h>
#include <file_provider.h>
#include <stdlib.h>
#include <stdio.h>
#include <bignum.h>
#include <rng.h>
#include <cryptoutil.h>
#include <aes.h>
#include <mp.h>
#include <gitbase.h>

int int_compare(int *a, int *b){
    return *a > *b;
}

void testQ(){
    std::vector<int>data = {1, 4, -2, 5, 200, -59};
    PriorityQueue<int> *pq = PriorityQueue_Create<int>(int_compare);
    for(uint i = 0; i < data.size(); i++){
        PriorityQueue_Push(pq, &data[i]);
    }

    while(PriorityQueue_Size(pq) > 0){
        int *s = PriorityQueue_Peek(pq);
        PriorityQueue_Pop(pq);
        printf("%d\n", *s);
    }

    PriorityQueue_Free(pq);

    exit(0);
}

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

void StartWithFile(const char *path=nullptr){
    BufferView *bView = AppGetActiveBufferView();
    BufferView_Initialize(bView, nullptr, EmptyView);

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

void CommandExecutorInit();
int main(int argc, char **argv){
    //testQ();
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
    AES_RunTestVector();
    return 0;
#endif

    DebuggerRoutines();

    CommandExecutorInit();

    if(argc > 1){
        char folder[PATH_MAX];
        char *p = argv[1];
        uint len = strlen(p);
        FileEntry entry;
        int r = GuessFileEntry(p, len, &entry, folder);

        if(r == -1){
            printf("Unknown argument\n");
            AppEarlyInitialize();
            StartWithFile();
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
