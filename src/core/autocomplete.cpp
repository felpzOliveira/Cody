#include <autocomplete.h>
#include <utilities.h>

static AutoCompleteBruteForce autoComplete_brute;
TernaryTreeNode *root = nullptr;

void AutoComplete_Initialize(){
    autoComplete_brute.alphabetList = List_Create<AutoCompleteEntry>();
    SymbolTable_Initialize(&autoComplete_brute.symTable, true);
}

void AutoComplete_Search(char *value, uint valuelen){
    int size = 0;
    List<AutoCompleteEntry> *searchList = autoComplete_brute.alphabetList;
    AutoCompleteEntry *e = nullptr;
    StringToCodepoint(value, valuelen, &size);
    auto locate = [&](AutoCompleteEntry *entry) -> int{
        if(StringEqual(value, entry->key.x, size)){
            return 1;
        }
        return 0;
    };

    e = List_Find<AutoCompleteEntry>(searchList, locate);
    if(e){
        ListNode<String> *ax = e->entries->head;
        while(ax != nullptr){
            String *e0 = ax->item;
            if(e0->size >= valuelen && e0->data){
                if(StringEqual(e0->data, value, valuelen)){
                    printf("Found match %s\n", e0->data);
                }
            }
            ax = ax->next;
        }
    }else{
        printf("No match\n");
    }
}

void AutoComplete_PushString(char *value, uint valuelen){
    return;
    int size = 0;
    List<AutoCompleteEntry> *searchList = autoComplete_brute.alphabetList;
    SymbolTable *symTable = &autoComplete_brute.symTable;
    AutoCompleteEntry *e = nullptr;
    // 1- Grab the first codepoint to check the size of the element
    if(valuelen < 2) return;
    int r = SymbolTable_Insert(symTable, value, valuelen, TOKEN_ID_NONE);
    
    if(r == 0) return;
    
    StringToCodepoint(value, valuelen, &size);
    auto locate = [&](AutoCompleteEntry *entry) -> int{
        if(StringEqual(value, entry->key.x, size)){
            return 1;
        }
        return 0;
    };

    TernarySearchTree_Insert(&root, value, valuelen);
    
    e = List_Find<AutoCompleteEntry>(searchList, locate);
    if(e){
        String *str = StringMake(value, valuelen);
        List_Push<String>(e->entries, str);
    }else{
        String *str = StringMake(value, valuelen);
        //printf("Inserting %s\n", str->data);
        
        e = AllocatorGetN(AutoCompleteEntry, 1);
        e->entries = List_Create<String>();
        Memcpy(e->key.x, value, size);
        List_Push<String>(e->entries, str);
        List_Push<AutoCompleteEntry>(searchList, e);
    }
}
