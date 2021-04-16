#include <file_provider.h>
#include <file_buffer.h>
#include <lex.h>
#include <utilities.h>
#include <app.h>
#include <file_base_hooks.h>

typedef struct FileProvider{
    FileBufferList fileBuffer;
    Tokenizer cppTokenizer;
    SymbolTable symbolTable;
    file_hook openHooks[MAX_HOOKS];
    file_hook creationHooks[MAX_HOOKS];
    uint openHooksCount, createHooksCount;
}FileProvider;

static FileProvider fProvider;

void FileProvider_Initialize(){
    FileBufferList_Init(&fProvider.fileBuffer);
    SymbolTable_Initialize(&fProvider.symbolTable);

    //TODO: Initialize all tokenizers
    fProvider.cppTokenizer = TOKENIZER_INITIALIZER;
    Lex_BuildTokenizer(&fProvider.cppTokenizer, appGlobalConfig.tabSpacing,
                       &fProvider.symbolTable);

    fProvider.openHooksCount = 0;
    fProvider.createHooksCount = 0;

    FileHooks_RegisterDefault();
}

int FileProvider_IsFileLoaded(char *path, uint len){
    return FileBufferList_FindByPath(&fProvider.fileBuffer, nullptr, path, len);
}

FileBufferList *FileProvider_GetBufferList(){
    return &fProvider.fileBuffer;
}

void FileProvider_Remove(char *ptr, uint pSize){
    FileBufferList_Remove(&fProvider.fileBuffer, ptr, pSize);
}

int FileProvider_FindByPath(LineBuffer **lBuffer, char *path, uint len, Tokenizer **tokenizer){
    *tokenizer = FileProvider_GuessTokenizer(path, len);
    return FileBufferList_FindByPath(&fProvider.fileBuffer, lBuffer, path, len);
}

Tokenizer *FileProvider_GetCppTokenizer(){
    return &fProvider.cppTokenizer;
}

// TODO: Implement when other languages are supported
Tokenizer *FileProvider_GuessTokenizer(char *filename, uint len){
    return FileProvider_GetCppTokenizer();
}

int FileProvider_IsFileOpened(char *path, uint len){
    return FileBufferList_FindByPath(&fProvider.fileBuffer, nullptr, path, len);
}

void FileProvider_CreateFile(char *targetPath, uint len, LineBuffer **lineBuffer,
                             Tokenizer **lTokenizer)
{
    Tokenizer *tokenizer = nullptr;
    LineBuffer *lBuffer = AllocatorGetN(LineBuffer, 1);
    *lBuffer = LINE_BUFFER_INITIALIZER;

    tokenizer = FileProvider_GuessTokenizer(targetPath, len);
    LineBuffer_InitEmpty(lBuffer);
    LineBuffer_SetStoragePath(lBuffer, targetPath, len);

    FileBufferList_Insert(&fProvider.fileBuffer, lBuffer);

    // allow hooks execution
    for(uint i = 0; i < fProvider.createHooksCount; i++){
        if(fProvider.creationHooks[i]){
            fProvider.creationHooks[i](targetPath, len, lBuffer, tokenizer);
        }
    }

    *lineBuffer = lBuffer;
    *lTokenizer = tokenizer;
}

void FileProvider_Load(char *targetPath, uint len, LineBuffer **lineBuffer,
                       Tokenizer **lTokenizer)
{
    LineBuffer *lBuffer = nullptr;
    Tokenizer *tokenizer = nullptr;
    uint fileSize = 0;
    char *content = nullptr;

    AssertA(lineBuffer != nullptr, "Invalid lineBuffer pointer");

    content = GetFileContents(targetPath, &fileSize);

    lBuffer = AllocatorGetN(LineBuffer, 1);
    *lBuffer = LINE_BUFFER_INITIALIZER;

    tokenizer = FileProvider_GuessTokenizer(targetPath, len);
    Lex_TokenizerContextReset(tokenizer);

    if(fileSize == 0){
        AllocatorFree(content);
        LineBuffer_InitEmpty(lBuffer);
        content = nullptr;
    }else{
        LineBuffer_Init(lBuffer, tokenizer, content, fileSize);
    }

    LineBuffer_SetStoragePath(lBuffer, targetPath, len);
    if(content != nullptr){
        AllocatorFree(content);
    }

    FileBufferList_Insert(&fProvider.fileBuffer, lBuffer);

    // allow hooks execution
    for(uint i = 0; i < fProvider.openHooksCount; i++){
        if(fProvider.openHooks[i]){
            fProvider.openHooks[i](targetPath, len, lBuffer, tokenizer);
        }
    }

    *lineBuffer = lBuffer;
    *lTokenizer = tokenizer;
}

void FileProvider_RegisterFileOpenHook(file_hook hook){
    if(fProvider.openHooksCount < MAX_HOOKS){
        fProvider.openHooks[fProvider.openHooksCount++] = hook;
    }
}

void FileProvider_RegisterFileCreateHook(file_hook hook){
    if(fProvider.createHooksCount < MAX_HOOKS){
        fProvider.creationHooks[fProvider.createHooksCount++] = hook;
    }
}
