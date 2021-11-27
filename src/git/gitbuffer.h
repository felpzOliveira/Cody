/* date = November 22nd 2021 21:29 */
#pragma once
#include <buffers.h>

/*
* Initializes the git internal buffers.
*/
void GitBuffer_InitializeInternalBuffer();

/*
* Request the global git linebuffer.
*/
LineBuffer *GitBuffer_GetLineBuffer();

/*
* Pushes a new line into the gitbuffer
*/
void GitBuffer_PushLine(char *line, uint size);

/*
* Clear the internal linebuffer.
*/
void GitBuffer_Clear();
