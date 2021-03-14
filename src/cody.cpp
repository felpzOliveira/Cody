#include <iostream>
#include <lex.h>
#include <buffers.h>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <bufferview.h>
#include <unistd.h>
#include <x11_display.h>
#include <app.h>
#include <hash.h>
#include <app.h>
#include <autocomplete.h>
#include <tries.h>

Tokenizer tokenizer = TOKENIZER_INITIALIZER;
SymbolTable symbolTable;

void test_trie(){
    Trie root = TRIE_NODE_INITIALIZER;
    char value[256];
    for(int i = 0; i < 80; i++){
        uint s = snprintf(value, 256, "String-%d", i);
        Trie_Insert(&root, value, s);
    }
    
    auto print_wd = [](char *buf, uint, Trie *node) -> void{
        printf("%s ( %u )\n", buf, node->word_count);
    };

    char *p = (char *)"String-1";
    uint len = strlen(p);
    char *ss = (char *)"String-15000";

    Trie_Insert(&root, ss, strlen(ss));
    Trie_Search(&root, p, len, print_wd);

    Trie_Remove(&root, ss, strlen(ss));
    Trie_Search(&root, p, len, print_wd);
    //Trie_Transverse(&root, print_wd);
}

void StartWithFile(const char *path=nullptr){
    AutoComplete_Initialize();
    SymbolTable_Initialize(&symbolTable);
    
    Lex_BuildTokenizer(&tokenizer, appGlobalConfig.tabSpacing, &symbolTable);
    int c = AppGetViewCount();
    for(int i = 0; i < c; i++){
        BufferView *view = AppGetBufferView(i);
        BufferView_Initialize(view, nullptr, &tokenizer);
    }
    
    if(path != nullptr){
        uint fileSize = 0;
        char *content = GetFileContents(path, &fileSize);
        BufferView *bView = AppGetActiveBufferView();
        if(fileSize == 0){
            AllocatorFree(content);
            content = AllocatorGetN(char, 1);
            content[0] = 0;
            fileSize = 1;
        }
        
        LineBuffer *lBuffer = AllocatorGetN(LineBuffer, 1);
        *lBuffer = LINE_BUFFER_INITIALIZER;
        LineBuffer_Init(lBuffer, bView->tokenizer, content, fileSize);
        LineBuffer_SetStoragePath(lBuffer, (char *)path, strlen(path));
        AllocatorFree(content);

        BufferView_SwapBuffer(bView, lBuffer);
        AppInsertLineBuffer(lBuffer);
    }
    
    Graphics_Initialize();
}

int main(int argc, char **argv){
    DebuggerRoutines();
#if 0
    test_trie();
    exit(0);
#endif
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
