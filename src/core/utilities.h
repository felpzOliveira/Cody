/* date = January 10th 2021 11:58 am */

#ifndef UTILITIES_H
#define UTILITIES_H
#include <types.h>
#include <geometry.h>
#include <string>
#include <fstream>
#include <signal.h>

#define MAX_STACK_SIZE 1024
#define MAX_BOUNDED_STACK_SIZE 16
#define DebugBreak() raise(SIGTRAP)

#define BreakIf(x, msg) if(!(x)){ printf("Break: %s: %s\n", #x, msg); DebugBreak(); }

const Float kInv255 = 0.003921569;

/* Reads a file and return its content in a new pointer */
char *GetFileContents(const char *path, uint *size);

/*
* Get the right-most value of a path string inside the path given.
* In case no one is found it returns -1 otherwise the right-most position located.
*/
int GetRightmostSplitter(const char *path, uint size);

/*
* Utility for getting a line from a opened std::istream.
*/
std::istream &GetNextLineFromStream(std::istream &is, std::string &str);

/*
* Simply utility for removing unwanted characters from a line '\n' and '\r' at the
* end only, it is a line after all.
*/
void RemoveUnwantedLineTerminators(std::string &line);

/*
* Checks if the character given is alphanumeric.
*/
int TerminatorChar(char v);

/*
* Checks if strings are equal.
*/
int StringEqual(char *s0, char *s1, uint maxn);


int FastStringSearch(char *s0, char *s1, uint s0len, uint s1len);

/*
* Gets the name path of the file without the full path.
*/
uint GetSimplifiedPathName(char *fullPath, uint len);

/*
* Convert a hex color to a unsigned vec3i/f color.
*/
vec4i ColorFromHex(uint hex);
vec4f ColorFromHexf(uint hex);

/*
* Count the amount of digits in a int.
*/
uint DigitCount(uint value);

/*
* Convert a codepoint to sequence of chars, i.e.: string,
* chars are written to 'c' and 'c' must be a vector with minimum size = 5.
* Returns the amount of bytes written into 'c'.
*/
int CodepointToString(int cp, char *c);

/*
* Gets the first codepoint in the string given by 'u' with size 'size'
* and return its integer value, returns the number of bytes taken to generate
* the codepoint in 'off'.
*/
int StringToCodepoint(char *u, int size, int *off);

/*
* Checks if a string contains a extensions, mostly for GLX/GL stuff
*/
int ExtensionStringContains(const char *string, const char *extensions);

/* memcpy */
void Memcpy(void *dst, void *src, uint size);
/* memset */
void Memset(void *dst, unsigned char v, uint size);

#endif //UTILITIES_H
