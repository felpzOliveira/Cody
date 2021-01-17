#include <iostream>
#include <lex.h>
#include <buffers.h>
#include <graphics.h>
#include <utilities.h>
#include <string.h>

void test_line_count(const char *path){
    uint filesize = 0;
    LineBuffer lineBuffer = LINE_BUFFER_INITIALIZER;
    char *p = GetFileContents(path, (uint *)&filesize);
    clock_t start, end;
    start = clock();
    LineBuffer_Init(&lineBuffer, p, filesize);
    end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    printf("Took %g\n", taken);
    
    Buffer *lastLine = &lineBuffer.lines[lineBuffer.lineCount-1];
    
    const char *testString = "This is a test string";
    
    Buffer_DebugStdoutData(lastLine);
    
    uint at = 28;
    Buffer_InsertStringAt(lastLine, at, (char *)testString, strlen(testString));
    Buffer_DebugStdoutData(lastLine);
    
    Buffer_RemoveRange(lastLine, at, at+strlen(testString));
    Buffer_DebugStdoutData(lastLine);
    
    at = lineBuffer.lineCount-4;
    printf("BEFORE =========================== [ %d ]\n", lineBuffer.lineCount);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-2);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+2);
    
    const char *testLineString = "This is a test string\n";
    LineBuffer_InsertLineAt(&lineBuffer, at, (char *)testLineString,
                            strlen(testLineString));
    printf("INSERTION =========================== [ %d ]\n", lineBuffer.lineCount);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-2);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+2);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+3);
    
    LineBuffer_RemoveLineAt(&lineBuffer, at);
    printf("DELETION =========================== [ %d ]\n", lineBuffer.lineCount);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-2);
    LineBuffer_DebugStdoutLine(&lineBuffer, at-1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+1);
    LineBuffer_DebugStdoutLine(&lineBuffer, at+2);
    
    LineBuffer_Free(&lineBuffer);
}

void test_line_processor(const char *path){
    auto testLineProcess = [](char **p, uint size, uint lineNr) -> void{
        printf("{ %d }: %s\n", (int)lineNr, *p);
    };
    
    Lex_LineProcess(path, testLineProcess);
    
}

//TODO: Investigate empty line results
Tokenizer tokenizer = TOKENIZER_INITIALIZER;
uint tokenCount = 0;
uint lineCount = 0;
void test_tokenizer(const char *filename){
    Lex_BuildTokenizer(&tokenizer);
    
    auto testLineProcess = [](char **p, uint size, uint lineNr) -> void{
        if(**p){
            Lex_TokenizerPrepareForNewLine(&tokenizer);
            printf("==============================================\n");
            printf("{ %d }: %s\n", (int)lineNr, *p);
            printf("==============================================\n");
            char *s = *p;
            
            int iSize = size;
            do{
                Token token;
                int rc = Lex_TokenizeNext(p, iSize, &token, &tokenizer);
                if(rc < 0){
                    iSize = 0;
                }else{
                    char f = token.value[token.size];
                    token.value[token.size] = 0;
                    printf("Token: %s { %s } [%d]\n", token.value,
                           Lex_GetIdString(token.identifier), token.position);
                    token.value[token.size] = f;
                    tokenCount ++;
                    iSize = size - token.position - token.size;
                }
            }while(iSize > 0 && **p != 0);
        }
        
        lineCount = lineNr;
    };
    
    clock_t start = clock();
    Lex_LineProcess(filename, testLineProcess);
    clock_t end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    printf("Tokens: %u, Lines: %u, Took %g\n", tokenCount, lineCount, taken);
}

int main(int argc, char **argv){
#if 0
    uint filesize = 0;
    char *data = GetFileContents("/home/felipe/Documents/Mayhem/test/simple.cpp",
                                 &filesize);
    char *p = (char *)&data[0];
    char *h = p;
    size_t len = 0;
    int rv = Lex_Comments(&p, filesize, &h, &len, nullptr);
    if(rv == 1){
        *p = 0;
        printf("Got string %s ( %d )\n", h, (int)len);
    }else{
        printf("No string >(\n");
    }
#endif
#if 0
    const char *ptr = "100000e+3";
    char *p = (char *)ptr;
    char *head = p;
    size_t size = 0;
    int v = Lex_Number(&p, strlen(ptr), &head, &size, nullptr);
    
    if(v){
        char data[256];
        memset(data, 0x00, sizeof(data));
        memcpy(data, head, size);
        printf("Accepted: %s\n", data);
    }else{
        printf("Not accepted: %s\n", ptr);
    }
#endif
#if 0
    test_line_count("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_line_count("/home/felipe/Documents/Mayhem/test/number.cpp");
    //(void)ReadFile("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //(void)ReadFile("/home/felipe/Documents/Mayhem/test/number.cpp");
#endif
#if 1
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    test_tokenizer("/home/felipe/Documents/Bubbles/src/generator/triangle.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
#endif
#if 0
    Graphics_Opts opts;
    opts.is_opengl = 1;
    
    Graphics_Initialize(&opts);
#endif
    
    return 0;
}