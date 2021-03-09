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

Tokenizer tokenizer = TOKENIZER_INITIALIZER;
SymbolTable symbolTable;

void test_autocomplete(){
    const char *value = "SomeData";
    uint len = strlen(value);
    AutoComplete_Initialize();
    AutoComplete_PushString((char *)value, len);
    AutoComplete_PushString((char *)value, len);
}

void test_ternary_tree(){
    TernaryTreeNode *root2 = nullptr;
        
    for(uint i = 0; i < 100; i++){
        char d[256];
        uint size = snprintf(d, sizeof(d), "STR-%d", i);
        TernarySearchTree_Insert(&root2, (char *)d, size);
    }
    
    auto print_words = [](const char *str) -> void{
        printf("Found word: %s\n", str);
    };
    
    TernarySearchTree_Transverse(root2, print_words);

    const char *s = "STR-9";
    uint sn = 5;
    char *out[30];
    int maxn = 30;
    int n = 0;

    void *p = TernarySearchTree_Guess(root2, (char *)s, sn, (char **)out, &n, maxn);
    if(p){
        printf("Got something\n");
        for(int i = 0; i < n; i++){
            printf(" > %s\n", out[i]);
        }
    }else{
        printf("No result\n");
    }
}

extern TernaryTreeNode *root;

void test_tokenizer(const char *path=nullptr){
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

        auto print_words = [](const char *str) -> void{
            printf("Found word: %s\n", str);
        };

        //TernarySearchTree_Transverse(root, print_words);

        BufferView_SwapBuffer(bView, lBuffer);
        AppInsertLineBuffer(lBuffer);
    }
    
    Graphics_Initialize();
}

int main(int argc, char **argv){
    DebuggerRoutines();
    IGNORE(CHDIR("/home/felipe/Documents/Mayhem/src"));
    AppEarlyInitialize();

    //test_ternary_tree();
    //exit(0);
    
    //test_autocomplete();
    
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer();
    test_tokenizer("/home/felipe/Documents/Mayhem/src/core/lex.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/src/core/lex.cpp");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/boundaries/lnm.h");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}
