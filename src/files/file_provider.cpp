#include <file_provider.h>
#include <file_buffer.h>
#include <lex.h>
#include <utilities.h>
#include <app.h>
#include <file_base_hooks.h>
#include <languages.h>
#include <storage.h>
#include <sstream>
#include <cryptoutil.h>
#include <aes.h>

typedef struct FileProvider{
    FileBufferList fileBuffer;
    SymbolTable symbolTable;
    file_hook openHooks[MAX_HOOKS];
    file_hook creationHooks[MAX_HOOKS];
    uint openHooksCount, createHooksCount;
    Tokenizer cppTokenizer, cppDetachedTokenizer;
    Tokenizer pyTokenizer, pyDetachedTokenizer;
    Tokenizer glslTokenizer, glslDetachedTokenizer;
    Tokenizer emptyTokenizer, emptyDetachedTokenizer;
    Tokenizer litTokenizer, litDetachedTokenizer;
    Tokenizer cmakeTokenizer, cmakeDetachedTokenizer;
    StorageDevice *storageDevice;
}FileProvider;

// Static allocation of the file provider
static FileProvider fProvider;

#if 0
static void DumplangFile(std::vector<std::vector<std::vector<GToken>> *> ss,
                         std::vector<const char *> titles, std::string path,
                         std::string target)
{
    std::stringstream pp;
    if(titles.size() != ss.size()){
        printf("Invalid size for language file\n");
        return;
    }

    uint i = 0;
    pp << "TARGET " << target << std::endl;
    for(std::vector<std::vector<GToken>> *k : ss){
        pp << "BEGIN " << titles[i++] << std::endl;
        for(std::vector<GToken> &ref : *k){
            for(GToken tk : ref){
                std::string name = Symbol_GetIdString(tk.identifier);
                if(name.size() == 0){
                    printf("MISSING TOKEN DEFINITION\n");
                    return;
                }
                name.erase(0, 9);
                pp << tk.value << " " << name << "\n";
            }
        }
        pp << "END" << std::endl;
    }

    std::ofstream ofs(path);
    if(ofs.is_open()){
        ofs << pp.str();
        ofs.close();
    }
}
#endif

static void InitTokenizers(){
    // NOTE: If this ever becomes slow we have 2 options:
    //        1- Use ParallelFor, I do believe we are able to initialize these
    //           in parallel.
    //        2- Initialize tokenizers only when their respective file is detected,
    //           this might be tricky, but might be the standard other editors do.

    //DumplangFile({&cppReservedPreprocessor, &cppReservedTable},
                //{"cppReservedPreprocessor", "cppReservedTable"}, "lang_cpp", "cpp.cpp");

    //DumplangFile({&glslReservedPreprocessor, &glslReservedTable},
                //{"glslReservedPreprocessor", "glslReservedTable"}, "lang_glsl", "glsl.cpp");

    //DumplangFile({&litReservedPreprocessor, &litReservedTable},
                //{"litReservedPreprocessor", "litReservedTable"}, "lang_lit", "lit.cpp");

    //DumplangFile({&cmakeReservedPreprocessor, &cmakeReservedTable},
                //{"cmakeReservedPreprocessor", "cmakeReservedTable"}, "lang_cmake", "cmake.cpp");

    // C/C++
    Lex_BuildTokenizer(&fProvider.cppTokenizer, &fProvider.symbolTable,
                       {&cppReservedPreprocessor, &cppReservedTable}, &cppSupport);

    Lex_BuildTokenizer(&fProvider.cppDetachedTokenizer, &fProvider.symbolTable,
                       {&cppReservedPreprocessor, &cppReservedTable}, &cppSupport);

    // Python
    Lex_BuildTokenizer(&fProvider.pyTokenizer, &fProvider.symbolTable,
                       {&pyReservedPreprocessor, &pyReservedTable}, &pythonSupport);

    Lex_BuildTokenizer(&fProvider.pyDetachedTokenizer, &fProvider.symbolTable,
                       {&pyReservedPreprocessor, &pyReservedTable}, &pythonSupport);

    // GLSL
    Lex_BuildTokenizer(&fProvider.glslTokenizer, &fProvider.symbolTable,
                       {&glslReservedPreprocessor, &glslReservedTable}, &glslSupport);

    Lex_BuildTokenizer(&fProvider.glslDetachedTokenizer, &fProvider.symbolTable,
                       {&glslReservedPreprocessor, &glslReservedTable}, &glslSupport);
    // LIT
    Lex_BuildTokenizer(&fProvider.litTokenizer, &fProvider.symbolTable,
                       {&litReservedPreprocessor, &litReservedTable}, &litSupport);

    Lex_BuildTokenizer(&fProvider.litDetachedTokenizer, &fProvider.symbolTable,
                       {&litReservedPreprocessor, &litReservedTable}, &litSupport);

    // Empty
    Lex_BuildTokenizer(&fProvider.emptyTokenizer, &fProvider.symbolTable,
                       {&noneReservedPreprocessor, &noneReservedTable}, &noneSupport);

    Lex_BuildTokenizer(&fProvider.emptyDetachedTokenizer, &fProvider.symbolTable,
                       {&noneReservedPreprocessor, &noneReservedTable}, &noneSupport);

    // Cmake
    Lex_BuildTokenizer(&fProvider.cmakeTokenizer, &fProvider.symbolTable,
                       {&cmakeReservedPreprocessor, &cmakeReservedTable}, &cmakeSupport);

    Lex_BuildTokenizer(&fProvider.cmakeDetachedTokenizer, &fProvider.symbolTable,
                       {&cmakeReservedPreprocessor, &cmakeReservedTable}, &cmakeSupport);
}

void FileProvider_Initialize(){
    FileBufferList_Init(&fProvider.fileBuffer);
    SymbolTable_Initialize(&fProvider.symbolTable, true);

    // Tokenizers all share the same Symbol Table as it is very expensive to have multiple
    fProvider.cppTokenizer   = TOKENIZER_INITIALIZER;
    fProvider.pyTokenizer    = TOKENIZER_INITIALIZER;
    fProvider.glslTokenizer  = TOKENIZER_INITIALIZER;
    fProvider.emptyTokenizer = TOKENIZER_INITIALIZER;
    fProvider.litTokenizer   = TOKENIZER_INITIALIZER;
    fProvider.cmakeTokenizer = TOKENIZER_INITIALIZER;

    // Detached tokenizer for parallel parsing
    fProvider.cppDetachedTokenizer   = TOKENIZER_INITIALIZER;
    fProvider.pyDetachedTokenizer    = TOKENIZER_INITIALIZER;
    fProvider.glslDetachedTokenizer  = TOKENIZER_INITIALIZER;
    fProvider.emptyDetachedTokenizer = TOKENIZER_INITIALIZER;
    fProvider.litDetachedTokenizer   = TOKENIZER_INITIALIZER;
    fProvider.cmakeDetachedTokenizer = TOKENIZER_INITIALIZER;

    InitTokenizers();

    fProvider.openHooksCount = 0;
    fProvider.createHooksCount = 0;

    fProvider.storageDevice = FetchStorageDevice();

    FileHooks_RegisterDefault();
}

int FileProvider_IsFileLoaded(char *path, uint len){
    return FileBufferList_FindByPath(&fProvider.fileBuffer, nullptr, path, len);
}

FileBufferList *FileProvider_GetBufferList(){
    return &fProvider.fileBuffer;
}

void FileProvider_Remove(LineBuffer *lineBuffer){
    FileBufferList_Remove(&fProvider.fileBuffer, lineBuffer);
}

void FileProvider_Remove(char *ptr, uint pSize){
    FileBufferList_Remove(&fProvider.fileBuffer, ptr, pSize);
}

int FileProvider_FindByPath(LineBuffer **lBuffer, char *path, uint len, Tokenizer **tokenizer){
    LineBufferProps props;
    if(tokenizer){
        *tokenizer = FileProvider_GuessTokenizer(path, len, &props);
    }
    return FileBufferList_FindByPath(&fProvider.fileBuffer, lBuffer, path, len);
}

// TODO: Implement when other languages are supported
Tokenizer *FileProvider_GuessTokenizer(char *filename, uint len,
                                       LineBufferProps *props, int detached)
{
    int p = GetFilePathExtension(filename, len);
    if(p > 0){
        char *ext = &filename[p];
        std::string strExt(ext);
        //TODO: Add as needed
        if(strExt == ".h" || strExt == ".hpp" || strExt == ".cpp"
           || strExt == ".c" || strExt == ".cc")
        {
            props->type = 0;
            props->ext = FILE_EXTENSION_CPP;
            if(detached) return FileProvider_GetDetachedCppTokenizer();
            return FileProvider_GetCppTokenizer();
        }else if(strExt == ".glsl" || strExt == ".gl" || strExt == ".vs" || strExt == ".fs"){
            props->type = 1;
            props->ext = FILE_EXTENSION_GLSL;
            if(detached) return FileProvider_GetDetachedGlslTokenizer();
            return FileProvider_GetGlslTokenizer();
        }else if(strExt == ".cu" || strExt == ".cuh"){
            props->type = 0;
            props->ext = FILE_EXTENSION_CUDA;
            if(detached) return FileProvider_GetDetachedCppTokenizer();
            return FileProvider_GetCppTokenizer();
        }else if(strExt == ".lit"){
            props->type = 3;
            props->ext = FILE_EXTENSION_LIT;
            if(detached) return FileProvider_GetDetachedLitTokenizer();
            return FileProvider_GetLitTokenizer();
        }

        else if(strExt == ".txt" || strExt == ".cmake"){
            int n = GetRightmostSplitter(filename, len);
            std::string val(&filename[n+1]);
            props->ext = FILE_EXTENSION_TEXT;
            if(val == "CMakeLists.txt" || strExt == ".cmake"){
                props->type = 4;
                if(detached) return FileProvider_GetDetachedCmakeTokenizer();
                return FileProvider_GetCmakeTokenizer();
            }

            props->type = 2;
            if(detached) return FileProvider_GetDetachedEmptyTokenizer();
            return FileProvider_GetEmptyTokenizer();
        }else if(strExt == ".py" || strExt == ".PY"){
            props->type = 5;
            props->ext = FILE_EXTENSION_PYTHON;
            if(detached) return FileProvider_GetDetachedPythonTokenizer();
            return FileProvider_GetPythonTokenizer();
        }

        // TODO: Cmake file should be handled here as well
    }

    props->type = 2;
    props->ext = FILE_EXTENSION_TEXT;
    if(detached) return FileProvider_GetDetachedEmptyTokenizer();
    return FileProvider_GetEmptyTokenizer();
}

int FileProvider_IsFileOpened(char *path, uint len){
    return FileBufferList_FindByPath(&fProvider.fileBuffer, nullptr, path, len);
}

Tokenizer *FileProvider_GetLineBufferTokenizer(LineBuffer *lineBuffer){
    uint id = LineBuffer_GetType(lineBuffer);
    switch(id){
        case 0: return FileProvider_GetCppTokenizer();
        case 1: return FileProvider_GetGlslTokenizer();
        case 2: return FileProvider_GetEmptyTokenizer();
        case 3: return FileProvider_GetLitTokenizer();
        case 4: return FileProvider_GetCmakeTokenizer();
        case 5: return FileProvider_GetPythonTokenizer();
        default:{
            //TODO: Empty tokenizer
            return FileProvider_GetEmptyTokenizer();
        }
    }
}

void FileProvider_CreateFile(char *targetPath, uint len, LineBuffer **lineBuffer,
                             Tokenizer **lTokenizer)
{
    LineBufferProps props;
    Tokenizer *tokenizer = nullptr;
    LineBuffer *lBuffer = AllocatorGetN(LineBuffer, 1);
    *lBuffer = LINE_BUFFER_INITIALIZER;

    tokenizer = FileProvider_GuessTokenizer(targetPath, len, &props);
    LineBuffer_InitEmpty(lBuffer);
    LineBuffer_SetStoragePath(lBuffer, targetPath, len);
    LineBuffer_SetType(lBuffer, props.type);
    LineBuffer_SetExtension(lBuffer, props.ext);
    LineBuffer_SetWrittable(lBuffer, true);

    FileBufferList_Insert(&fProvider.fileBuffer, lBuffer);

    // allow hooks execution
    for(uint i = 0; i < fProvider.createHooksCount; i++){
        if(fProvider.creationHooks[i]){
            fProvider.creationHooks[i](targetPath, len, lBuffer, tokenizer);
        }
    }

    if(lineBuffer) *lineBuffer = lBuffer;
    if(lTokenizer) *lTokenizer = tokenizer;
}

bool FileProvider_VerifyEncryptedFile(uint8_t *content, uint32_t size){
    uint8_t head[] = {CRYPTO_MAGIC};
    uint8_t salt[CRYPTO_SALT_LEN];
    // 1- No magic + iv
    if(size < (7 + CRYPTO_SALT_LEN + 16 + 1))
        return false;

    // 2- No header
    if(memcmp(content, head, 7) != 0)
        return false;

    return true;
}

static char *tempContent = nullptr;
static uint tempSize = 0;
static std::string tempPath;

void FileProvider_ReleaseTemporary(){
    if(tempContent)
        AllocatorFree(tempContent);
    tempSize = 0;
    tempPath = std::string();
    tempContent = nullptr;
}

int FileProvider_LoadTemporary(char *pwd, uint pwdlen, LineBuffer **lineBuffer,
                               bool mustFinish)
{
    LineBuffer *lBuffer = nullptr;
    Tokenizer *tokenizer = nullptr;
    char *targetPath = (char *)tempPath.c_str();
    uint len = (uint)tempPath.size();
    LineBufferProps props;

    uint8_t salt[CRYPTO_SALT_LEN];
    uint8_t iv[16];
    uint8_t key[32];
    uint8_t *input = nullptr;
    std::vector<uint8_t> decrypted;

    if(tempContent == nullptr || tempSize < (7 + CRYPTO_SALT_LEN + 16)){
        FileProvider_ReleaseTemporary();
        return FILE_LOAD_FAILED;
    }

    // copy the salt
    memcpy(salt, &tempContent[7], CRYPTO_SALT_LEN);

    // copy the iv
    memcpy(iv, &tempContent[7 + CRYPTO_SALT_LEN], 16);

    // generate key
    CryptoUtil_PasswordHash(pwd, pwdlen, key, salt);

    input = (uint8_t *)&tempContent[7 + CRYPTO_SALT_LEN + 16];
    if(!AES_CBC_Decrypt(input, tempSize - 7 - CRYPTO_SALT_LEN - 16, key,
                        iv, AES256, decrypted))
    {
        printf("Could not decrypt file\n");
        FileProvider_ReleaseTemporary();
        return FILE_LOAD_FAILED;
    }

    uint fileSize = (uint)decrypted.size();
    char *content = AllocatorGetN(char, fileSize+1);

    memcpy(content, (char *)decrypted.data(), fileSize);
    content[fileSize] = 0;

    lBuffer = AllocatorGetN(LineBuffer, 1);
    *lBuffer = LINE_BUFFER_INITIALIZER;

    tokenizer = FileProvider_GuessTokenizer(targetPath, len, &props, 1);
    Lex_TokenizerContextReset(tokenizer);

    LineBuffer_SetStoragePath(lBuffer, targetPath, len);

    FileBufferList_Insert(&fProvider.fileBuffer, lBuffer);
    LineBuffer_SetType(lBuffer, props.type);
    LineBuffer_SetExtension(lBuffer, props.ext);
    LineBuffer_SetWrittable(lBuffer, true);
    LineBuffer_SetEncrypted(lBuffer, key, salt);

    if(fileSize == 0 || content == nullptr){
        AllocatorFree(content);
        LineBuffer_InitEmpty(lBuffer);
        content = nullptr;
    }else{
        LineBuffer_Init(lBuffer, tokenizer, content, fileSize, mustFinish);
    }

    if(content != nullptr){
        // NOTE: the memory taken from the file is released when the lexer
        // has finished processing the contents of the file. Since now we
        // are using the DisptachExecution method for loading files to handle
        // large loads it is not safe to free this memory here.

        //AllocatorFree(content);
    }

    // allow hooks execution
    for(uint i = 0; i < fProvider.openHooksCount; i++){
        if(fProvider.openHooks[i]){
            fProvider.openHooks[i](targetPath, len, lBuffer, tokenizer);
        }
    }

    if(lineBuffer){
        *lineBuffer = lBuffer;
    }

    FileProvider_ReleaseTemporary();
    return FILE_LOAD_SUCCESS;
}

int FileProvider_Load(char *targetPath, uint len, LineBuffer **lineBuffer,
                      bool mustFinish)
{
    LineBuffer *lBuffer = nullptr;
    Tokenizer *tokenizer = nullptr;
    uint fileSize = 0;
    LineBufferProps props;
    char *content = nullptr;
    StorageDevice *device = fProvider.storageDevice;

    FileProvider_ReleaseTemporary();

    tempContent = nullptr;
    tempSize = 0;

    if(device){
        uint8_t *ptr = nullptr;
        content = device->GetContentsOf(targetPath, &fileSize);

        ptr = (uint8_t *)content;
        if(ptr != nullptr){
            // NOTE: Check for encrypted file
            if(FileProvider_VerifyEncryptedFile(ptr, (uint32_t)fileSize)){
                // TODO: Need to interrupt this, ask for password, decrypt and than
                //       swap buffers. The question here is how do we maintain control
                //       in this routine if we have to yield to the query bar at this moment?
                tempContent = content;
                tempSize = fileSize;
                tempPath = std::string(targetPath, len);
                return FILE_LOAD_REQUIRES_DECRYPT;
            }
        }
    }

    lBuffer = AllocatorGetN(LineBuffer, 1);
    *lBuffer = LINE_BUFFER_INITIALIZER;

    tokenizer = FileProvider_GuessTokenizer(targetPath, len, &props, 1);
    Lex_TokenizerContextReset(tokenizer);

    LineBuffer_SetStoragePath(lBuffer, targetPath, len);

    FileBufferList_Insert(&fProvider.fileBuffer, lBuffer);
    LineBuffer_SetType(lBuffer, props.type);
    LineBuffer_SetExtension(lBuffer, props.ext);
    LineBuffer_SetWrittable(lBuffer, true);
    lBuffer->props.isEncrypted = false;

    if(fileSize == 0 || content == nullptr){
        AllocatorFree(content);
        LineBuffer_InitEmpty(lBuffer);
        content = nullptr;
    }else{
        LineBuffer_Init(lBuffer, tokenizer, content, fileSize, mustFinish);
    }

    if(content != nullptr){
        // NOTE: the memory taken from the file is released when the lexer
        // has finished processing the contents of the file. Since now we
        // are using the DisptachExecution method for loading files to handle
        // large loads it is not safe to free this memory here.

        //AllocatorFree(content);
    }

    // allow hooks execution
    for(uint i = 0; i < fProvider.openHooksCount; i++){
        if(fProvider.openHooks[i]){
            fProvider.openHooks[i](targetPath, len, lBuffer, tokenizer);
        }
    }

    if(lineBuffer){
        *lineBuffer = lBuffer;
    }

    return FILE_LOAD_SUCCESS;
}

int FileProvider_IsLineBufferDirty(char *hint_name, uint len){
    LineBuffer *lineBuffer = nullptr;
    int r = FileBufferList_FindByName(&fProvider.fileBuffer, &lineBuffer,
                                      hint_name, len);
    if(r && lineBuffer != nullptr){
        return lineBuffer->is_dirty;
    }

    return 0;
}

void RegisterFileOpenHook(file_hook hook){
    if(fProvider.openHooksCount < MAX_HOOKS){
        fProvider.openHooks[fProvider.openHooksCount++] = hook;
    }
}

void RegisterFileCreateHook(file_hook hook){
    if(fProvider.createHooksCount < MAX_HOOKS){
        fProvider.creationHooks[fProvider.createHooksCount++] = hook;
    }
}

SymbolTable *FileProvider_GetSymbolTable(){
    return &fProvider.symbolTable;
}

Tokenizer *FileProvider_GetCppTokenizer(){
    return &fProvider.cppTokenizer;
}

Tokenizer *FileProvider_GetPythonTokenizer(){
    return &fProvider.pyTokenizer;
}

Tokenizer *FileProvider_GetGlslTokenizer(){
    return &fProvider.glslTokenizer;
}

Tokenizer *FileProvider_GetEmptyTokenizer(){
    return &fProvider.emptyTokenizer;
}

Tokenizer *FileProvider_GetLitTokenizer(){
    return &fProvider.litTokenizer;
}

Tokenizer *FileProvider_GetCmakeTokenizer(){
    return &fProvider.cmakeTokenizer;
}

Tokenizer *FileProvider_GetDetachedCppTokenizer(){
    return &fProvider.cppDetachedTokenizer;
}

Tokenizer *FileProvider_GetDetachedGlslTokenizer(){
    return &fProvider.glslDetachedTokenizer;
}

Tokenizer *FileProvider_GetDetachedEmptyTokenizer(){
    return &fProvider.emptyDetachedTokenizer;
}

Tokenizer *FileProvider_GetDetachedPythonTokenizer(){
    return &fProvider.pyDetachedTokenizer;
}

Tokenizer *FileProvider_GetDetachedLitTokenizer(){
    return &fProvider.litDetachedTokenizer;
}

Tokenizer *FileProvider_GetDetachedCmakeTokenizer(){
    return &fProvider.cmakeDetachedTokenizer;
}
