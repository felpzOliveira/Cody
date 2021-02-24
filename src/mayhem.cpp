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

Tokenizer tokenizer = TOKENIZER_INITIALIZER;
LineBuffer lineBuffer = LINE_BUFFER_INITIALIZER;

void test_hash(const char *filename){
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
    
    AllocatorFree(content);
    
    struct _HashPair{
        std::string value;
        uint64 hash;
        uint64 colisions;
        uint64 copies;
    };
    
    const uint size = 100000;
    uint seed = 0x811c9dc5;
    
    uint64 hashed = 0;
    uint tableSize = 0;
    _HashPair *table = new _HashPair[size];
    
    for(uint i = 0; i < lineBuffer.lineCount; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(&lineBuffer, i);
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(token->identifier != TOKEN_ID_SPACE){
                char *str = &buffer->data[token->position];
                char v = buffer->data[token->position + token->size];
                buffer->data[token->position + token->size] = 0;
                
                uint64 hash = FowlerNollVoStringHash64(str);
                //uint64 hash = MurmurHash3(str, token->size, seed);
                
                uint64 id = hash % size;
                hashed++;
                
                _HashPair *entry = &table[id];
                std::string target(str);
                if(entry->value.size() > 0){
                    if(entry->value != target){
                        entry->colisions ++;
                    }else{
                        entry->copies++;
                    }
                }else{
                    entry->value = target;
                    entry->hash = hash;
                    entry->colisions = 0;
                    entry->copies = 0;
                    tableSize++;
                }
                
                buffer->data[token->position + token->size] = v;
            }
        }
    }
    std::cout << "Table Size: " << tableSize << ", hashed: " << hashed << std::endl;
    
#if 0
    for(uint i = 0; i < size; i++){
        _HashPair *entry = &table[i];
        if(entry->value.size() > 0){
            std::cout << entry->value << " = " << entry->copies << " = " << 
                entry->colisions << std::endl;
        }
    }
#endif
    uint maxColisions = 0;
    uint line = 0;
    for(uint i = 0; i < size; i++){
        _HashPair *entry = &table[i];
        if(entry->value.size() > 0){
            if(entry->colisions > 0 && entry->colisions > maxColisions){
                maxColisions = entry->colisions;
                line = i;
            }
        }
    }
    
    if(maxColisions > 0){
        _HashPair *entry = &table[line];
        std::cout << entry->value << " = " << entry->copies << " = " << 
            entry->colisions << std::endl;
    }else{
        std::cout << "No Collisions!" << std::endl;
    }
    
    delete[] table;
}

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
    
    AllocatorFree(content);
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
    //test_hash("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    //test_hash("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_hash("/home/felipe/Documents/Mayhem/src/graphics/graphics.cpp");
    //test_hash("/home/felipe/Documents/Bubbles/src/boundaries/lnm.h");
    
    //test_tokenizer("/home/felipe/Downloads/sqlite-amalgamation-3340000/sqlite3.c");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/core/geometry.h");
    test_tokenizer("/home/felipe/Documents/Mayhem/src/graphics/graphics.cpp");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/boundaries/lnm.h");
    //test_tokenizer("/home/felipe/Documents/Bubbles/src/bubbles.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/simple.cpp");
    //test_tokenizer("/home/felipe/Documents/Mayhem/test/empty.cpp");
    return 0;
}
