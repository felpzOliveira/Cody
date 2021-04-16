#pragma once
#include <types.h>
#include <file_buffer.h>

/*
* File provider controls how files are provided to the editor
*/
#define MAX_HOOKS 16

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
Tokenizer *FileProvider_GetGlslTokenizer();

/*
* Asks the file provider to guess what is the tokenizer to use for a given file
*/
Tokenizer *FileProvider_GuessTokenizer(char *filename, uint len);

/*
* Queries the file provider if a file is already opened.
*/
int FileProvider_IsFileOpened(char *path, uint len);

/*
* Loads a file given its path. lineBuffer returns a new line buffer for the file
* and lTokenizer returns the guessesd tokenizer.
*/
void FileProvider_Load(char *targetPath, uint len, LineBuffer **lineBuffer,
                       Tokenizer **lTokenizer);

/*
* Creates a new file. lineBuffer returns a new line buffer allocated for the file
* and lTokenizer returns the guessed tokenizer.
*/
void FileProvider_CreateFile(char *targetPath, uint len, LineBuffer **lineBuffer,
                             Tokenizer **lTokenizer);

/*
* Register a file open hook.
*/
void RegisterFileOpenHook(file_hook hook);

/*
* Register a file creation hook.
*/
void RegisterFileCreateHook(file_hook hook);
