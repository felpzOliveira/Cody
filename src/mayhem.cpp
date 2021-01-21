#include <iostream>
#include <lex.h>
#include <buffers.h>
#include <graphics.h>
#include <utilities.h>
#include <string.h>
#include <bufferview.h>
#include <unistd.h>

void test_line_processor(const char *path){
    uint filesize = 0;
    char *content = GetFileContents(path, &filesize);
    auto testLineProcess = [](char **p, uint size, uint lineNr, void *prv) -> void{
        printf("{ %d }: %s\n", (int)lineNr, *p);
    };
    
    Lex_LineProcess(content, filesize, testLineProcess);
    AllocatorFree(content);
}

extern BufferView bufferView;
void test_tokenizer(const char *filename){
    uint filesize = 0;
    Tokenizer tokenizer = TOKENIZER_INITIALIZER;
    LineBuffer lineBuffer = LINE_BUFFER_INITIALIZER;
    char *content = GetFileContents(filename, &filesize);
    
    BufferView_InitalizeFromText(&bufferView, content, filesize);
    
#if 1
    Graphics_Initialize();
    
    while(1){ sleep(5); }
#endif
    LineBuffer_Free(&lineBuffer);
}

int main(int argc, char **argv){
#if 0
    test_line_count("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_line_count("/home/felipe/Documents/Mayhem/test/number.cpp");
    //(void)ReadFile("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //(void)ReadFile("/home/felipe/Documents/Mayhem/test/number.cpp");
#endif
#if 1
    test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
#endif
#if 0
    Graphics_Opts opts;
    opts.is_opengl = 1;
    
    Graphics_Initialize(&opts);
#endif
    
    return 0;
}