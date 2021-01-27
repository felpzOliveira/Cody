#include <iostream>
#include <lex.h>
#include <buffers.h>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <bufferview.h>
#include <unistd.h>
#include <x11_display.h>

extern BufferView bufferView;
void test_tokenizer(const char *filename){
    uint filesize = 0;
    Tokenizer tokenizer = TOKENIZER_INITIALIZER;
    LineBuffer lineBuffer = LINE_BUFFER_INITIALIZER;
    char *content = GetFileContents(filename, &filesize);
    
    if(filesize == 0){ // TODO: Solution for empty file?
        AllocatorFree(content);
        content = AllocatorGetN(char, 1);
        content[0] = 0;
        filesize = 1;
    }
    
    BufferView_InitalizeFromText(&bufferView, content, filesize);
    
#if 1
    Graphics_Initialize();
    
    while(Graphics_IsRunning()){ sleep(1); }
#endif
    LineBuffer_Free(&lineBuffer);
}


int main(int argc, char **argv){
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}