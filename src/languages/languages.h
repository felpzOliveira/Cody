/* date = February 24th 2021 2:12 pm */

#ifndef LANGUAGES_H
#define LANGUAGES_H
#include <symbol.h>
#include <vector>

// auxiliary structure for building the token table.
typedef struct{
    const char *value;
    TokenId identifier;
}GToken; // TODO: At least TRY to parse from this and see if it is ok

// C/C++
extern std::vector<std::vector<GToken>> cppReservedPreprocessor;
extern std::vector<std::vector<GToken>> cppReservedTable;

// GLSL
extern std::vector<std::vector<GToken>> glslReservedPreprocessor;
extern std::vector<std::vector<GToken>> glslReservedTable;

#endif //LANGUAGES_H
