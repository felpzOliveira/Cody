#include <iostream>
#include <lex.h>
#include <buffers.h>
#include <graphics.h>
#include <utilities.h>
#include <string.h>

#define TOKEN_TERMINATORS " ,;(){}+-/|><!~:%&*=\n\r\t\0"

static char *LookAhead(char **ptr, size_t n, const char *tokens, 
                       size_t tokenLen, size_t *len, int *tokenId)
{
    char *head = *ptr;
    *len = 0;
    *tokenId = -1;
    for(size_t i = 0; i < n; i++){
        for(size_t k = 0; k < tokenLen; k++){
            if((int)(**ptr) == (int)(tokens[k])){
                *tokenId = (int)k;
                return head;
            }
        }
        (*ptr)++;
        (*len)++;
        if(!*ptr) return nullptr;
    }
    
    return nullptr;
}

#define LEX_PROCESSOR(name) int name(char **p, size_t n, char **head, size_t *len)
LEX_PROCESSOR(Lex_Number);

//TODO:Fix
static char *ReadFile(const char *path){
    uint bytes = 0, size = 0;
    int id = 0;
    char *token = nullptr;
    const char *tokenList = TOKEN_TERMINATORS;
    int tsize = strlen(tokenList);
    clock_t start, end;
    
    char *ptr = GetFileContents(path, &bytes);
    
    if(!ptr) return nullptr;
    char *p = ptr;
    
    start = clock();
    do{
        size_t len = 0;
        int anyToken = Lex_Number(&p, bytes, &token, &len);
        if(!anyToken){
            int id = 0;
            token = LookAhead(&p, bytes, tokenList, tsize, &len, &id);
            if(token){
                bytes -= len + 1;
#if 0
                *p = '\0';
                printf("Skipped: %s\n", token);
                *p = tokenList[id];
#endif
                p++;
            }
        }else{
#if 0
            char data[256];
            memset(data, 0x00, sizeof(data));
            memcpy(data, token, len);
            printf("Token %s\n", data);
#endif
            bytes -= len;
        }
    }while(bytes > 0 && token != nullptr);
    
    end = clock();
    double taken = (double)((end - start)) / (double)CLOCKS_PER_SEC;
    printf("Took %g\n", taken);
    
    return nullptr; //TODO
}

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
    auto testLineProcess = [](char **p, unsigned int size) -> int{
        static int lineID = 0;
        printf("{ %d }: %s\n", lineID++, *p);
        return 1;
    };
    
    Lex_LineProcess(path, testLineProcess);
    
}

int main(int argc, char **argv){
#if 0
    const char *ptr = "100000e+3";
    char *p = (char *)ptr;
    char *head = p;
    size_t size = 0;
    int v = Lex_Number(&p, strlen(ptr), &head, &size);
    
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
    test_line_processor("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
#endif
    
    
    Graphics_Opts opts;
    opts.is_opengl = 1;
    
    Graphics_Initialize(&opts);
    
    while(1){}
    return 0;
}