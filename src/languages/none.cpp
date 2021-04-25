#include <languages.h>

TokenizerSupport noneSupport = {
    .comments = true,
    .strings = true,
    .numbers = true,
    .lookups = true,
};

/* Empty token list */
std::vector<std::vector<GToken>> noneReservedTable = {};
std::vector<std::vector<GToken>> noneReservedPreprocessor = {};
