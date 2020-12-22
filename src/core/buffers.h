/* date = December 21st 2020 7:58 pm */

#ifndef BUFFERS_H
#define BUFFERS_H
#include <types.h>

/*
* Basic data structure for lines. data holds the line pointer
* size the current available size for the data pointer and count
* the amount of characters in use, data pointer is not necessarily 
* null terminated, i.e.: '\0'.
*/
typedef struct{
    uint size;
    uint count;
    char *data;
}Buffer;

/*
* Basic description of a structured file. A list of lines with the available size
* and current line count.
*/
typedef struct{
    Buffer *lines;
    uint lineCount;
    uint size;
}LineBuffer;

/* For static initialization */
#define BUFFER_INITIALIZER {.size = 0, .count = 0, .data = nullptr}
#define LINE_BUFFER_INITIALIZER {.lines = nullptr, .lineCount = 0, .size = 0}

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
* Removes a range of the given Buffer. Indexes are given by 'start' and 'end' such
* that start < end.
*/
void Buffer_RemoveRange(Buffer *buffer, uint start, uint end);

/*
* Releases a Buffer, giving its memory back.
*/
void Buffer_Free(Buffer *buffer);

/*
* Initializes a LineBuffer from the contents of a file given in 'fileContents' with size
* 'filesize'.
*/
void LineBuffer_Init(LineBuffer *lineBuffer, const char *fileContents, uint filesize);

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
* Removes a line at a position 'at' of the LineBuffer.
*/
void LineBuffer_RemoveLineAt(LineBuffer *lineBuffer, uint at);

/*
* Releases memory taken by a LineBuffer.
*/
void LineBuffer_Free(LineBuffer *lineBuffer);


/* Debug stuff */
void Buffer_DebugStdoutData(Buffer *buffer);
void LineBuffer_DebugStdoutLine(LineBuffer *lineBuffer, uint lineNo);

#endif //BUFFERS_H
