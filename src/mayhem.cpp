#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

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

static char *GetFileContents(const char *path, size_t *size){
    struct stat st;
    size_t filesize = 0, bytes = 0;
    char *ret = nullptr;
    
    int fd = open(path, O_RDONLY);
    if(fd == -1) goto _error;
    
    stat(path, &st);
    filesize = st.st_size;
    
    posix_fadvise(fd, 0, 0, 1);
    
    ret = (char *)malloc(filesize);
    if(!ret) goto _error_memory;
    
    bytes = read(fd, ret, filesize);
    if(bytes != filesize) goto _error_memory;
    
    close(fd);
    *size = bytes;
    return ret;
    
    _error_memory:
    if(ret){
        free(ret);
        ret = nullptr;
    }
    
    _error:
    if(fd >= 0) close(fd);
    return ret;
}


#define LEX_PROCESSOR(name) int name(char **p, size_t n, char **head, size_t *len)
LEX_PROCESSOR(Lex_Number);

//TODO:Fix
static char *ReadFile(const char *path){
    size_t bytes = 0, size = 0;
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

int main(int argc, char **argv){
    std::cout << "Hello Mayhem[2] " << std::endl;
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
#else
    (void)ReadFile("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //(void)ReadFile("/home/felipe/Documents/Mayhem/test/number.cpp");
#endif
    return 0;
}