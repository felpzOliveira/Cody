/* date = December 21st 2020 7:58 pm */

#ifndef BUFFERS_H
#define BUFFERS_H
#include <types.h>
#include <lex.h>
#include <undo.h>
#include <symbol.h>
#include <vector>
#include <encoding.h>
#include <cryptoutil.h>

/*
* Basic data structure for lines. data holds the line pointer,
* size the current available size for the data pointer and taken
* the amount of characters in use, data pointer is not necessarily 
* null terminated, i.e.: '\0'. Tokens pointer gets addresses for
* the logical descriptor of the line and are provided by the 'Lex_TokenizeNext'
* call of the parser. We also keep track of a tokenizer state 'stateContext'
* that is used to determine how to start processing this line upon updates.
* The 'count' parameter refers to indexed position in UTF-8, i.e.: if this
* buffer is being used for printing or if you don't want to handle offsets
* decoding UTF-8 you can loop using 'count' instead. Every time a update
* happens in the buffer this variable is updated and shows the amount
* of encoded UTF-8 characters available in the buffer.
*/
struct Buffer{
    uint size;
    uint count;
    uint taken;
    char *data;
    Token *tokens;
    uint tokenCount;
    bool is_ours;
    bool erased;
    TokenizerStateContext stateContext;
};


typedef enum{
    FILE_EXTENSION_NONE = 0,
    FILE_EXTENSION_CPP,
    FILE_EXTENSION_PYTHON,
    FILE_EXTENSION_GLSL,
    FILE_EXTENSION_CUDA,
    FILE_EXTENSION_LIT,
    FILE_EXTENSION_LIT_DARK, // ???
    FILE_EXTENSION_LIT_WHITE, // ???
    FILE_EXTENSION_CMAKE,
    FILE_EXTENSION_TEXT,
    FILE_EXTENSION_FONT,
    FILE_EXTENSION_FOLDER,

    FILE_EXTENSION_LOCK_DARK,
    FILE_EXTENSION_LOCK_WHITE,
    FILE_EXTENSION_TOTAL,
}FileExtension;

typedef struct{
    vec2ui start, end;
    Float currTime;
    Float interval;
    int active;
}CopySection;

struct LineBufferProps{
    uint type;
    FileExtension ext;
    CopySection cpSection;
    bool isWrittable;
    bool isInternal;
    bool isEncrypted;
    uint8_t key[32];
    uint8_t salt[CRYPTO_SALT_LEN];
    EncoderDecoder encoder;
};

/*
* Basic description of a structured file. A list of lines with the available size and
* current line count.
*/
struct LineBuffer{
    Buffer **lines;
    char filePath[PATH_MAX];
    uint filePathSize;
    uint lineCount;
    uint size;
    uint is_dirty;
    UndoRedo undoRedo;
    vec2i activeBuffer;
    LineBufferProps props;
};

/* For static initialization */
#define BUFFER_INITIALIZER {.size = 0, .count = 0, .taken = 0, .data = nullptr, .tokens = nullptr, .tokenCount = 0, .is_ours = false }
#define LINE_BUFFER_INITIALIZER {.lines = nullptr, .lineCount = 0, .size = 0,}

/*
* NOTE: All functions that accept values inside the buffer for inserting or removing
*       assume the input is in UTF-8 values and decode the position when execing.
*       If you wish to access the data inside the buffer at a specific location
*       use 'Buffer_Utf8PositionToRawPosition' to get the actual initial byte
*       location corresponding to a UTF-8 character.
*/

/*
* Initializes a previously allocated Buffer to the size specified in 'size'.
*/
void Buffer_Init(Buffer *buffer, uint size);

/*
* Marks this buffer as being modified by our editor. While we automatically call
* this function in several places during editing because of the nature of this
* operation it is better for you to call this manually when you are about to edit
* a buffer. This is used to better format the output file when writing to disk.
*/
void Buffer_Claim(Buffer *buffer);

/*
* Locates the first non empty token inside a given buffer.
*/
int Buffer_FindFirstNonEmptyToken(Buffer *buffer);

/*
* Locates the first non empty position inside a given buffer.
*/
uint Buffer_FindFirstNonEmptyLocation(Buffer *buffer);

/*
* Initializes a previously allocated Buffer from a string pointer given in 'head'
* with size 'len'.
*/
void Buffer_InitSet(Buffer *buffer, char *head, uint len, EncoderDecoder *encoder);

/*
* Inserts a new string in a Buffer at a fixed position 'at'. String is given in 'str'
* and must have length 'len'. The position 'at' is a UTF-8
* encoded position. Returns the amount of bytes actually inserted after tab expansion.
*/
uint Buffer_InsertStringAt(Buffer *buffer, uint at, char *str, uint len, EncoderDecoder *encoder);

/*
* Inserts a new string in a Buffer at a fixed position 'at'. String is given in 'str'
* and must have length 'len'. This function receives the 'at' position in raw bytes, you
* must know where is correct to start the writing in the buffer.
* Returns the amount of bytes actually inserted after tab expansion.
*/
uint Buffer_InsertRawStringAt(Buffer *buffer, uint at, char *str, uint len, EncoderDecoder *encoder);

/*
* Updates the curret token list inside the buffer given. This function copies
* the referenced token list.
*/
void Buffer_UpdateTokens(Buffer *buffer, Token *tokens, uint size);

/*
* Removes a range of the given Buffer. Indexes are given by 'start' and 'end' such
* that start < end. Range is given in UTF-8 positions.
*/
void Buffer_RemoveRange(Buffer *buffer, uint start, uint end, EncoderDecoder *encoder);

/*
* Logically erase the contents of the buffer, i.e.: erase the contents of the
* buffer without freeing memory. Equivalent to:
*        Buffer_RemoveRangeRaw(buffer,  0, buffer->taken);
* but this also handles zeroing tokens.
*/
void Buffer_SoftClear(Buffer *buffer, EncoderDecoder *encoder);

/*
* Removes the last token from a buffer. Erases both the token and the data
* related to the token.
*/
void Buffer_RemoveLastToken(Buffer *buffer, EncoderDecoder *encoder);

/*
* Removes a range of the given Buffer. Range is given in raw position inside the
* buffer.
*/
void Buffer_RemoveRangeRaw(Buffer *buffer, uint start, uint end, EncoderDecoder *encoder);

/*
* Checks if a buffer is visually empty when rendering.
*/
int Buffer_IsBlank(Buffer *buffer);

/*
* Locates the next valid position for jumping locations in utf-8 positions.
* This routine computes the next jump point for iterating the buffer based
* on the terminators given in 'term'.
*/
uint Buffer_FindNextWord8(Buffer *buffer, uint u8, const char *term,
                          uint len, EncoderDecoder *encoder);

/*
* Locates the previous valid position for jumping locations in utf-8 positions.
*/
uint Buffer_FindPreviousWordU8(Buffer *buffer, uint u8, const char *term,
                               uint len, EncoderDecoder *encoder);

/*
* Locates the previous valid position for jumping separators.
*/
uint Buffer_FindPreviousSeparator(Buffer *buffer, uint rawp);

/*
* Locates the next valid position for jumping separators.
*/
uint Buffer_FindNextSeparator(Buffer *buffer, uint rawp);

/*
* Perform cleanup of the entries of the symbol table **and** auto completes
* ternary search tree relating to tokens inside this buffer.
*/
void Buffer_EraseSymbols(Buffer *buffer, SymbolTable *symTable);

/*
* Count the amount of encoded characters.
*/
uint Buffer_GetUtf8Count(Buffer *buffer, EncoderDecoder *encoder);

/*
* Get the id of the token that contains position 'u8'.
*/
uint Buffer_GetTokenAt(Buffer *buffer, uint u8, EncoderDecoder *encoder);

/*
* Get the raw position inside the buffer corresponding to a UTF-8 position,
* in case 'len' is not nullptr it returns the length in bytes of the current
* UTF-8 encoded position. The position returned can be seen as a starting position
* for printing or looping a UTF-8 character.
*/
uint Buffer_Utf8PositionToRawPosition(Buffer *buffer, uint u8p, int *len, EncoderDecoder *encoder);

/*
* Get the UTF-8 position inside this buffer matching a raw position. This is mostly
* usefull if you want to get the rendering position of a token or adjust a cursor
* movement based on the rendering property of a encoded character.
*/
uint Buffer_Utf8RawPositionToPosition(Buffer *buffer, uint rawp, EncoderDecoder *encoder);

/*
* Adjust the position to **make sure** it does not lie inside a tab expansion.
* The direction variable can be passed to tell to which side of a tab it should
* clamp in case it does indeed lie inside a tab expansion, it has value -1 for
* clamping to the left and +1 for clamping to the right.
*/
uint Buffer_PositionTabCompensation(Buffer *buffer, uint rawp, int direction);

/*
* Copy the contents of 'src' into 'dst' duplicating the 'src' buffer.
*/
void Buffer_CopyDeep(Buffer *dst, Buffer *src);

/*
* Generates tokens without tokenization. This simply splits the strings
* and generates TOKEN_ID_NONE and TOKEN_ID_SPACE values to force a buffer
* to be renderable without needing to perform tokenization and symbol table lookups.
*/
void Buffer_FastTokenGen(Buffer *buffer);

/*
* Releases a Buffer, giving its memory back.
*/
void Buffer_Free(Buffer *buffer);

/*
* Initializes a LineBuffer without any contents for composition parsing.
*/
void LineBuffer_InitBlank(LineBuffer *lineBuffer);

/*
* Initializes a LineBuffer without any contents, i.e.: empty file.
*/
void LineBuffer_InitEmpty(LineBuffer *lineBuffer);

/*
* Initializes a LineBuffer from the contents of a file given in 'fileContents' with size
* 'filesize'. This routine can parse through the file in parallel using the flag
* 'synchronous', note that when building the LineBuffer in parallel the contents of the
* file must not be freed until the LineBuffer is marked as writtable. Note that when
* parsing in parallel, by default, the host is only dispatched back when at least 500
* lines have been parsed. This is done so that if the file is immediatly rendered
* it has a minimal size, during this process the file is marked as read-only.
*/
void LineBuffer_Init(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                     char *fileContents, uint filesize, bool synchronous=true);

/*
* Inserts a new line at the end of the LineBuffer. The line is specified by its contents
* in 'line' with size 'size'.
*/
void LineBuffer_InsertLine(LineBuffer *lineBuffer, char *line, uint size);

/*
* Inserts a new line at a position 'at' of the LineBuffer. The line is 
* specified by its contents in 'line' with size 'size'.
*/
void LineBuffer_InsertLineAt(LineBuffer *lineBuffer, uint at, char *line, uint size);

/*
* Inserts a (possible) multiline UTF-8 text at position 'at'. Returns the amount
* of lines that were actually inserted. Returns the ending offset of the last
* line inserted.
*/
uint LineBuffer_InsertRawTextAt(LineBuffer *lineBuffer, char *text, uint size,
                                uint base, uint u8offset, uint *offset,
                                int replaceDashR=0);

/*
* Inserts a (possible) multiline UTF-8 text at the end of a linebuffer.
* Returns the amount of lines that were actually inserted.
*/
uint LineBuffer_InsertRawText(LineBuffer *lineBuffer, char *text, uint size);

/*
* Merge two lines of the LineBuffer, merges line (base+1) into base.
*/
void LineBuffer_MergeConsecutiveLines(LineBuffer *lineBuffer, uint base);

/*
* Updates the token list present in the 'lineNr' to be 'tokens'.
*/
void LineBuffer_CopyLineTokens(LineBuffer *lineBuffer, uint lineNr,
                               Token *tokens, uint size);
/*
* Removes a line at a position 'at' of the LineBuffer.
*/
void LineBuffer_RemoveLineAt(LineBuffer *lineBuffer, uint at);

/*
* Returns the buffer containing the line at 'lineNo' or nullptr if no one exists.
*/
Buffer *LineBuffer_GetBufferAt(LineBuffer *lineBuffer, uint lineNo);

/*
* Forces the current tokenizer to re-compute tokens starting from base.
* Depending on the Buffer located at 'base' the Tokenizer might work on
* previous Buffers (context->backTrack) or later ones (context->forwardTrack).
* Optionally can also pass a offset to allow for extension on the computed
* termination range, usefull for when adding new lines.
*/
void LineBuffer_ReTokenizeFromBuffer(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                                     uint base, uint offset);

/*
* Generates tokens without tokenization. This simply splits the strings
* and generates TOKEN_ID_NONE and TOKEN_ID_SPACE values to force a linebuffer
* to be renderable without needing to perform tokenization and symbol table lookups.
*/
void LineBuffer_FastTokenGen(LineBuffer *lineBuffer, uint base, uint offset);

/*
* Updates the linebuffer to include its diff content. Returns in 'range' the
* range of modified lines as (start, end). This routine processes the raw deltas
* contained in the linebuffer and transforms it into displayable text that can be rendered.
*/
void LineBuffer_InsertDiffContent(LineBuffer *lineBuffer, vec2ui &range);

/*
* Generates the raw text from a range with the contents of this linebuffer.
* Interior formatted tabs are removed from this string, this routine allocates
* the pointer 'ptr'.
*/
uint LineBuffer_GetTextFromRange(LineBuffer *lineBuffer, char **ptr,
                                 vec2ui start, vec2ui end);

/*
* Clears a linebuffer wihtout actually deleting data, it just marks as free.
*/
void LineBuffer_SoftClear(LineBuffer *lineBuffer);

/*
* Gets the storage path for the contents of the LineBuffer given.
*/
char *LineBuffer_GetStoragePath(LineBuffer *lineBuffer);

/*
* Sets the storage path for the contents of the LineBuffer given.
*/
void LineBuffer_SetStoragePath(LineBuffer *lineBuffer, char *path, uint size);

/*
* Sets flag indicating if a given linebuffer is writtable.
* NOTE: this is a flag used for external operations the buffer
*       interface does not care about this value.
*/
void LineBuffer_SetWrittable(LineBuffer *lineBuffer, bool isWrittable);

/*
* Checks if the is writtable flag is set on a given linebuffer.
*/
bool LineBuffer_IsWrittable(LineBuffer *lineBuffer);

/*
* Saves the contents of the lineBuffer to storage.
*/
bool LineBuffer_SaveToStorage(LineBuffer *lineBuffer);

/*
* Saves the content of the lineBuffer to storage encrypted.
*/
bool LineBuffer_SaveToStorageEncrypted(LineBuffer *lineBuffer);

/*
* Sets the active buffer during edit. This allows for buffering and prevent
* unecessary memory and stack usage for undo operations.
*/
void LineBuffer_SetActiveBuffer(LineBuffer *lineBuffer, vec2i bufId, int safe=1);

/*
* Returns the id of the active buffer during edit.
*/
vec2i LineBuffer_GetActiveBuffer(LineBuffer *lineBuffer);

/*
* Replaces the buffer located at 'at' and returns the existing buffer.
*/
Buffer *LineBuffer_ReplaceBufferAt(LineBuffer *lineBuffer, Buffer *buffer, uint at);

/*
* Generic setter for the linebuffer properties.
*/
void LineBuffer_SetType(LineBuffer *lineBuffer, uint type);
void LineBuffer_SetExtension(LineBuffer *lineBuffer, FileExtension ext);
void LineBuffer_SetCopySection(LineBuffer *lineBuffer, CopySection section);
void LineBuffer_SetInternal(LineBuffer *lineBuffer);
void LineBuffer_SetEncrypted(LineBuffer *lineBuffer, uint8_t *key, uint8_t *salt);

/*
* Generic getter for the linebuffer properties.
*/
uint LineBuffer_GetType(LineBuffer *lineBuffer);
FileExtension LineBuffer_GetExtension(LineBuffer *lineBuffer);
void LineBuffer_GetCopySection(LineBuffer *lineBuffer, CopySection *section);
bool LineBuffer_IsInternal(LineBuffer *lineBuffer);
bool LineBuffer_IsEncrypted(LineBuffer *lineBuffer);

/*
* Checks if a token inside a buffer in the given linebuffer is inside the
* current copy section (in case it is active).
*/
int LineBuffer_IsInsideCopySection(LineBuffer *lineBuffer, uint id, uint bid);

/*
* Advances the copy section (if required) of a given linebuffer by dt.
*/
void LineBuffer_AdvanceCopySection(LineBuffer *lineBuffer, double dt);

/*
* Gets a pointer to the encoder for this linebuffer.
*/
EncoderDecoder *LineBuffer_GetEncoderDecoder(LineBuffer *lineBuffer);

/*
* Informs the linebuffer it needs to update its lines because the encoder it was bounded
* to changed.
*/
void LineBuffer_UpdateEncoding(LineBuffer *lineBuffer);

/*
* Releases memory taken by a LineBuffer.
*/
void LineBuffer_Free(LineBuffer *lineBuffer);

/* Helpers */

/*
* Allocates a linebuffer for internal usage. It is marked with proper
* flags indicating it does not represent a file. Optionally can receive an
* already allocated linebuffer to initialize instead of allocating.
* Returns the resulting linebuffer.
*/
LineBuffer *LineBuffer_AllocateInternal(LineBuffer *lBuffer);

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer);
void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo);
uint LineBuffer_DebugLoopAllTokens(LineBuffer *lineBuffer, const char *m, uint size);
void LineBuffer_DebugPrintRange(LineBuffer *lineBuffer, vec2i range);

#endif //BUFFERS_H
