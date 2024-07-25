#pragma once
#include <types.h>
#include <lex.h>
#include <file_buffer.h>

/*
* File provider controls how files are provided to the editor
*/
#define MAX_HOOKS 16

#define FILE_LOAD_FAILED           0
#define FILE_LOAD_SUCCESS          1
#define FILE_LOAD_REQUIRES_DECRYPT 2
#define FILE_LOAD_REQUIRES_VIEWER  3

#define FILE_TYPE_ON_LOAD_TEXT  0
#define FILE_TYPE_ON_LOAD_IMAGE 1

// Function pointer for file hooks
typedef void(*file_hook)(char *filepath, uint len, LineBuffer *lineBuffer, Tokenizer *tokenizer);

/*
* Initializes the file provider API.
*/
void FileProvider_Initialize();

/*
* Checks if a given file is already loaded.
*/
int FileProvider_IsFileLoaded(char *path, uint len);

/*
* Gets the file buffer list from the provider.
*/
FileBufferList *FileProvider_GetBufferList();

/*
* Asks the file provider to remove a file
*/
void FileProvider_Remove(char *path, uint len);
void FileProvider_Remove(LineBuffer *lineBuffer);

/*
* Finds the linebuffer (if any) that holds a file given by its path, it also returns
* the guessed tokenizer for the file.
*/
int FileProvider_FindByPath(LineBuffer **lBuffer, char *path, uint len,
                            Tokenizer **tokenizer);

/*
* Checks if the linebuffer holding the file given by its hint name is currently dirty.
*/
int FileProvider_IsLineBufferDirty(char *hint_name, uint len);

/*
* Asks the file provider for a specific tokenizer
*/
Tokenizer *FileProvider_GetCppTokenizer();
Tokenizer *FileProvider_GetPythonTokenizer();
Tokenizer *FileProvider_GetGlslTokenizer();
Tokenizer *FileProvider_GetEmptyTokenizer();
Tokenizer *FileProvider_GetLitTokenizer();
Tokenizer *FileProvider_GetCmakeTokenizer();
Tokenizer *FileProvider_GetTexTokenizer();
Tokenizer *FileProvider_GetDetachedCppTokenizer();
Tokenizer *FileProvider_GetDetachedPythonTokenizer();
Tokenizer *FileProvider_GetDetachedGlslTokenizer();
Tokenizer *FileProvider_GetDetachedEmptyTokenizer();
Tokenizer *FileProvider_GetDetachedLitTokenizer();
Tokenizer *FileProvider_GetDetachedCmakeTokenizer();
Tokenizer *FileProvider_GetDetachedTexTokenizer();

/*
* Asks the file provider to guess what is the tokenizer to use for a given file,
* it also returns properties of a linebuffer that would hold this file.
*/
Tokenizer *FileProvider_GuessTokenizer(char *filename, uint len,
                                       LineBufferProps *props, int detached=0);

/*
* Queries the file provider for the tokenizer that should be used with the given
* linebuffer.
*/
Tokenizer *FileProvider_GetLineBufferTokenizer(LineBuffer *lineBuffer);

/*
* Queries the file provider for the active symbol table.
*/
SymbolTable *FileProvider_GetSymbolTable();

/*
* Queries the file provider if a file is already opened.
*/
int FileProvider_IsFileOpened(char *path, uint len);

/*
* Loads a file given its path. lineBuffer returns a new line buffer for the file
* The 'mustFinish' flag indicates if the load should not run asynchronously,
* i.e.: at return the file must be completely loaded and all used resources must
* be available. Settings 'mustFinish' to false allows for asynchronous load and
* faster return, this allows for rendering the file wihout it being fully loaded,
* usefull for large files but dangerous for loading files in sequence, use with care.
* Returns whether or not it was possible to load the file.
*/
int FileProvider_Load(char *targetPath, uint len, int &type,
                      LineBuffer **lineBuffer=nullptr, bool mustFinish=true);

/*
* Loads an encrypted file that was stored as temporary during the file open operation.
*/
int FileProvider_LoadTemporary(char *pwd, uint pwdlen, int &type, LineBuffer **lineBuffer,
                               bool mustFinish);

/*
* Gets the raw content that was loaded and cached, either this is not text
* or it is encrypted.
*/
char *FileProvider_GetTemporary(uint *len);

/*
* Erase the temporary data.
*/
void FileProvider_ReleaseTemporary();

/*
* Creates a new file. lineBuffer returns a new line buffer allocated for the file
* and lTokenizer returns the guessed tokenizer. Both can be null if a reference
* to the creation is not required.
*/
void FileProvider_CreateFile(char *targetPath, uint len, LineBuffer **lineBuffer,
                             Tokenizer **lTokenizer);

/*
* Register a file open hook. You are not allowed to read the file on this hook.
* File opening **might** be performed in a dispatch execution meaning that its
* content might not be available when your hook is called because it is being
* processed in a different thread.
*/
void RegisterFileOpenHook(file_hook hook);

/*
* Register a file creation hook.
*/
void RegisterFileCreateHook(file_hook hook);
