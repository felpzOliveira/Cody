/* date = February 4th 2021 6:16 pm */

#ifndef FONT_H
#define FONT_H

/*
* Generic public API for font consulting.
* Must be provided somewhere so App can decide what to do with a input.
*/
int Font_SupportsCodepoint(int codepoint);

#endif //FONT_H
