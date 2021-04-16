#include <iostream>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <unistd.h>
#include <app.h>
#include <file_provider.h>

void StartWithFile(const char *path=nullptr){
    BufferView *bView = AppGetActiveBufferView();
    BufferView_Initialize(bView, nullptr, nullptr);

    if(path != nullptr){
        LineBuffer *lineBuffer = nullptr;
        Tokenizer *tokenizer = nullptr;
        uint length = strlen(path);

        FileProvider_Load((char *)path, length, &lineBuffer, &tokenizer);
        BufferView_SwapBuffer(bView, lineBuffer, tokenizer);
    }
    
    Graphics_Initialize();
}

int main(int argc, char **argv){
    DebuggerRoutines();
    
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

    //test_ternary_tree();
    //exit(0);
    
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer();
    //test_tokenizer("/home/felipe/Documents/Mayhem/src/core/lex.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/src/core/lex.cpp");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/boundaries/lnm.h");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}
