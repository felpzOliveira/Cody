#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <unistd.h>
#include <app.h>
#include <file_provider.h>
#include <stdlib.h>
#include <stdio.h>

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
    BufferView_Initialize(bView, nullptr);

    if(path != nullptr){
        LineBuffer *lineBuffer = nullptr;
        uint length = strlen(path);

        FileProvider_Load((char *)path, length, &lineBuffer, false);
        BufferView_SwapBuffer(bView, lineBuffer);
    }else{
        LoadStaticFilesOnStart();
    }

    Graphics_Initialize();
}

void CommandExecutorInit();
int main(int argc, char **argv){
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
