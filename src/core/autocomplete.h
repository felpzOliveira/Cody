#if !defined(AUTO_COMPLETE_H)
#define AUTO_COMPLETE_H

#include <utilities.h>
#include <symbol.h>

/* Lets atttempt to implement auto-complete shall we? */

//Brute force this thing

struct AutoCompleteEntry{
    CharU8 key;
    List<String> *entries;
};

struct AutoCompleteBruteForce{
    List<AutoCompleteEntry> *alphabetList;
    SymbolTable symTable;
};

void AutoComplete_Initialize();

void AutoComplete_PushString(char *value, uint valuelen);

void AutoComplete_Search(char *value, uint valuelen);

#endif
