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

Tokenizer tokenizer = TOKENIZER_INITIALIZER;
LineBuffer lineBuffer = LINE_BUFFER_INITIALIZER;

void test_tokenizer2(const char *filename){
    uint filesize = 0;
    char *content = GetFileContents(filename, &filesize);
    const char *search = "vec4";
    
    FastStringSearch((char *)search, content, strlen(search), filesize);
}

void test_tokenizer(const char *filename){
    uint filesize = 0;
    char *content = GetFileContents(filename, &filesize);
    uint len = strlen("tmpfile.h");
    
    if(filesize == 0){ // TODO: Solution for empty file?
        AllocatorFree(content);
        content = AllocatorGetN(char, 1);
        content[0] = 0;
        filesize = 1;
    }
    
    Lex_BuildTokenizer(&tokenizer, appGlobalConfig.tabSpacing);
    LineBuffer_Init(&lineBuffer, &tokenizer, content, filesize);
    LineBuffer_SetStoragePath(&lineBuffer, (char *)"tmpfile.h", len);
    
#if 0
    clock_t start = clock();
    uint s = LineBuffer_DebugLoopAllTokens(&lineBuffer, "sqlite3", 7);
    clock_t end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    printf("Took %g ( %u )\n\n", taken, s);
    
    exit(0);
#endif
    
    int c = AppGetBufferViewCount();
    for(int i = 0; i < c; i++){
        BufferView *view = AppGetBufferView(i);
        BufferView_Initialize(view, &lineBuffer, &tokenizer);
    }
    
    
    //SymbolTable_DebugPrint(&tokenizer.symbolTable);
    Graphics_Initialize();
    
    //while(Graphics_IsRunning()){ sleep(1); }
}

int main(int argc, char **argv){
    AppEarlyInitialize();
    
    test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}
