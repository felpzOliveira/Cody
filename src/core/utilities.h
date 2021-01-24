/* date = January 10th 2021 11:58 am */

#ifndef UTILITIES_H
#define UTILITIES_H
#include <types.h>
#include <geometry.h>
#include <string>
#include <fstream>

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

/*
* Convert a hex color to a unsigned vec3i color.
*/
vec3i ColorFromHex(uint hex);
vec3f ColorFromHexf(uint hex);

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
* and return its integer value, returns the number of bytes tooked from 'u'
* in 'off'.
*/
int StringToCodepoint(char *u, int size, int *off);

int ExtensionStringContains(const char *string, const char *extensions);
#endif //UTILITIES_H
