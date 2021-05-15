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

/*
* Initializes the auto-complete interface keyboard binding.
*/
BindingMap *AutoComplete_Initialize();

/*
* Pushes a string into the auto-complete trie structure.
*/
void AutoComplete_PushString(char *value, uint valuelen);

/*
* Queries the auto-complete interface for suggestions to be inserted in
* a given selectable list based on the string given in 'value'.
*/
int AutoComplete_Search(char *value, uint valuelen, SelectableList *list);

/*
* Removes a string from the auto-complete trie structure.
*/
void AutoComplete_Remove(char *value, uint valuelen);

/*
* Applies the current selected suggested item to the active view.
*/
void AutoComplete_Commit();

#endif
