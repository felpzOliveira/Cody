/* date = February 24th 2021 2:12 pm */

#ifndef LANGUAGES_H
#define LANGUAGES_H
#include <symbol.h>
#include <vector>
#include <functional>

// auxiliary structure for building the token table.
typedef struct{
    const char *value;
    TokenId identifier;
}GToken; // TODO: At least TRY to parse from this and see if it is ok

// context aware parsing
struct ContextualProcessor{
    std::string start, end;
    TokenId id;
    int is_multiline;
    // when multiline = 2 this informs if the capture
    // should continue
    std::function<bool(char *, char *)> fnChecker;
};

typedef struct{
    bool comments;
    bool strings;
    bool numbers;
    bool lookups;
    bool functions;
    bool multilineComment;
    char lineCommentChar;
    std::vector<ContextualProcessor> procs;
}TokenizerSupport;

// C/C++
extern std::vector<std::vector<GToken>> cppReservedPreprocessor;
extern std::vector<std::vector<GToken>> cppReservedTable;
extern TokenizerSupport cppSupport;

// GLSL
extern std::vector<std::vector<GToken>> glslReservedPreprocessor;
extern std::vector<std::vector<GToken>> glslReservedTable;
extern TokenizerSupport glslSupport;

// LIT
extern std::vector<std::vector<GToken>> litReservedPreprocessor;
extern std::vector<std::vector<GToken>> litReservedTable;
extern TokenizerSupport litSupport;

// Python
extern std::vector<std::vector<GToken>> pyReservedPreprocessor;
extern std::vector<std::vector<GToken>> pyReservedTable;
extern TokenizerSupport pythonSupport;

// Empty
extern std::vector<std::vector<GToken>> noneReservedPreprocessor;
extern std::vector<std::vector<GToken>> noneReservedTable;
extern TokenizerSupport noneSupport;

// Cmake
extern std::vector<std::vector<GToken>> cmakeReservedTable;
extern std::vector<std::vector<GToken>> cmakeReservedPreprocessor;
extern TokenizerSupport cmakeSupport;

// Tex
extern std::vector<std::vector<GToken>> texReservedTable;
extern std::vector<std::vector<GToken>> texReservedPreprocessor;
extern TokenizerSupport texSupport;

#endif //LANGUAGES_H
