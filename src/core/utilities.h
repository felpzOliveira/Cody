/* date = January 10th 2021 11:58 am */

#ifndef UTILITIES_H
#define UTILITIES_H
#include <types.h>
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
#endif //UTILITIES_H
