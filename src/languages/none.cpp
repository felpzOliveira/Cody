#include <languages.h>

TokenizerSupport noneSupport = {
    .comments = false,
    .strings = true,
    .numbers = true,
    .lookups = false,
};

/* Empty token list */
std::vector<std::vector<GToken>> noneReservedTable = {};
std::vector<std::vector<GToken>> noneReservedPreprocessor = {};
