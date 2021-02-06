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
void test_tokenizer(const char *filename){
    
    uint filesize = 0;
    char *content = GetFileContents(filename, &filesize);
    
    if(filesize == 0){ // TODO: Solution for empty file?
        AllocatorFree(content);
        content = AllocatorGetN(char, 1);
        content[0] = 0;
        filesize = 1;
    }
    
    Lex_BuildTokenizer(&tokenizer);
    LineBuffer_Init(&lineBuffer, &tokenizer, content, filesize);
    
    int c = AppGetBufferViewCount();
    for(int i = 0; i < c; i++){
        BufferView *view = AppGetBufferView(i);
        BufferView_Initialize(view, &lineBuffer, &tokenizer);
    }
    
#if 1
    Graphics_Initialize();
    
    while(Graphics_IsRunning()){ sleep(1); }
#endif
}


int main(int argc, char **argv){
    AppEarlyInitialize();
    
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    test_tokenizer("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}
