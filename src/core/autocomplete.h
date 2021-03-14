#if !defined(AUTO_COMPLETE_H)
#define AUTO_COMPLETE_H

#include <utilities.h>
#include <symbol.h>
#include <selectable.h>
#include <view.h>
#include <keyboard.h>
#include <tries.h>

/* Lets atttempt to implement auto-complete shall we? */

struct AutoComplete{
    uint lastSearchLen;
    char *lastSearchValue;
    BindingMap *mapping;
    Trie root;
};

BindingMap *AutoComplete_Initialize();

void AutoComplete_PushString(char *value, uint valuelen);

int AutoComplete_Search(char *value, uint valuelen, SelectableList *list);

void AutoComplete_Remove(char *value, uint valuelen);

int AutoComplete_Activate(View *view);


void AutoComplete_Commit();
#endif
