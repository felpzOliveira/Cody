/* date = December 21st 2020 7:58 pm */

#ifndef BUFFERS_H
#define BUFFERS_H
#include <types.h>
#include <lex.h>

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
typedef struct{
    uint size;
    uint count;
    uint taken;
    char *data;
    Token *tokens;
    uint tokenCount;
    TokenizerStateContext stateContext;
}Buffer;

/*
* Basic description of a structured file. A list of lines with the available size and
* current line count.
*/
typedef struct{
    Buffer *lines;
    uint lineCount;
    uint size;
}LineBuffer;

/* For static initialization */
#define BUFFER_INITIALIZER {.size = 0, .count = 0, .data = nullptr, .tokens = nullptr, .tokenCount = 0}
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
* Initializes a previously allocated Buffer from a string pointer given in 'head'
* with size 'len'.
*/
void Buffer_InitSet(Buffer *buffer, char *head, uint len);

/*
* Inserts a new string in a Buffer at a fixed position 'at'. String is given in 'str'
* and must have length 'len'.
*/
void Buffer_InsertStringAt(Buffer *buffer, uint at, char *str, uint len);

/*
* Updates the curret token list inside the buffer given. This function copies
* the referenced token list.
*/
void Buffer_UpdateTokens(Buffer *buffer, Token *tokens, uint size);

/*
* Removes a range of the given Buffer. Indexes are given by 'start' and 'end' such
* that start < end.
*/
void Buffer_RemoveRange(Buffer *buffer, uint start, uint end);

/*
* Checks whether a buffer is visually empty.
*/
int Buffer_IsBlank(Buffer *buffer);

/*
* Count the amount of UTF-8 characters.
*/
uint Buffer_GetUtf8Count(Buffer *buffer);

/*
* Get the raw position inside the buffer corresponding to a UTF-8 position,
* in case 'len' is not nullptr it returns the length in bytes of the current
* UTF-8 encoded position. The position returned can be seen as a starting position
* for printing or looping a UTF-8 character.
*/
uint Buffer_Utf8PositionToRawPosition(Buffer *buffer, uint u8p, int *len=nullptr);

/*
* Get the UTF-8 position inside this buffer matching a raw position. This is mostly
* usefull if you want to get the rendering position of a token.
*/
uint Buffer_Utf8RawPositionToPosition(Buffer *buffer, uint rawp);

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
* 'filesize'.
*/
void LineBuffer_Init(LineBuffer *lineBuffer, Tokenizer *tokenizer,
                     char *fileContents, uint filesize);

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
* Releases memory taken by a LineBuffer.
*/
void LineBuffer_Free(LineBuffer *lineBuffer);

/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer);
void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo);

#endif //BUFFERS_H
