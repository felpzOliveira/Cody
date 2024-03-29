#include <symbol.h>
#include <hash.h>
#include <utilities.h>
#include <string.h>
#include <autocomplete.h>

inline int _symbol_table_sym_node_matches(SymbolNode *node, char *label,
                                          uint labelLen, TokenId id)
{
    if(node == nullptr) return 0;
    if(node->label == nullptr || label == nullptr) return 0;
    if(node->labelLen != labelLen) return 0;
    if(node->id != id) return 0;
    return StringEqual(node->label, label, labelLen);
}

inline uint _symbol_table_hash(SymbolTable *symTable, char *label, uint labelLen){
    /*
    * NOTE: The MurmurHash3 algorithm has shown to be actually slower than FowlerNollVo
    *       however it is better in avoiding collisions,
    *       but feel free to experiment with this.
    */
    //return FowlerNollVoStringHash32(label);
    return MurmurHash3(label, labelLen, symTable->seed);
}

void SymbolTable_Initialize(SymbolTable *symTable, bool duplicate){
    symTable->table = AllocatorGetN(SymbolNode*, SYMBOL_TABLE_SIZE);
    symTable->tableSize = SYMBOL_TABLE_SIZE;
    symTable->seed = 0x811c9dc5; // TODO: rand
    symTable->allow_duplication = duplicate;
    Memset(symTable->table, 0x00, sizeof(SymbolNode*) * SYMBOL_TABLE_SIZE);
}

int SymbolTable_Insert(SymbolTable *symTable, char *label, uint labelLen, TokenId id){
    uint insert_id = 0;
    SymbolNode *newNode = nullptr;

    if(!(labelLen > AutoCompleteMinInsertLen) || !symTable) return 1;

    uint hash = _symbol_table_hash(symTable, label, labelLen);
    uint index = hash % symTable->tableSize;

    SymbolNode *node = symTable->table[index];
    SymbolNode *prev = nullptr;

    while(node != nullptr){
        if(_symbol_table_sym_node_matches(node, label, labelLen, id)){
            if(symTable->allow_duplication){
                node->duplications++;
                return 1;
            }
            return 0;
        }

        prev = node;
        node = node->next;
        insert_id++;
    }

    // create a new entry
    newNode = AllocatorGetN(SymbolNode, 1);
    newNode->label = StringDup(label, labelLen);
    newNode->labelLen = labelLen;
    newNode->id = id;
    newNode->next = nullptr;
    newNode->prev = nullptr;
    newNode->duplications = 0;

    if(insert_id > 0){
        prev->next = newNode;
        newNode->prev = prev;
    }else{
        symTable->table[index] = newNode;
    }

    //printf("Inserted %s - %s\n", newNode->label, Symbol_GetIdString(newNode->id));
    return 1;
}

void SymbolTable_Remove(SymbolTable *symTable, char *label, uint labelLen, TokenId id){
    uint tableIndex;
    if(!(labelLen > AutoCompleteMinInsertLen) || !symTable) return;

    SymbolNode *node = SymbolTable_GetEntry(symTable, label, labelLen, id, &tableIndex);
    if(node){
        if(symTable->allow_duplication){
            if(node->duplications > 0){
                node->duplications--;
                return;
            }
        }

        SymbolNode *prev = node->prev;
        if(prev == nullptr){ // head
            symTable->table[tableIndex] = node->next;
            if(node->next)
                node->next->prev = nullptr;
        }else{ // middle
            prev->next = node->next;
            if(node->next)
                node->next->prev = prev;
        }

        //printf("Removed %s - %s\n", node->label, Symbol_GetIdString(node->id));

        node->next = nullptr;
        AllocatorFree(node->label);
        AllocatorFree(node);
    }
}

SymbolNode *SymbolTable_GetEntry(SymbolTable *symTable, char *label, uint labelLen,
                                 TokenId id, uint *tableIndex)
{
    SymbolNode *nodeRes = nullptr;
    if(!symTable) return nullptr;

    uint hash = _symbol_table_hash(symTable, label, labelLen);
    uint index = hash % symTable->tableSize;
    SymbolNode *node = symTable->table[index];

    *tableIndex = index;

    while(node != nullptr){
        if(node->label){
            if(node->labelLen == labelLen && node->id == id){
                if(StringEqual(label, node->label, labelLen)){
                    nodeRes = node;
                    break;
                }
            }
        }

        node = node->next;
    }

    return nodeRes;
}

SymbolNode *SymbolTable_Search(SymbolTable *symTable, char *label, uint labelLen){
    SymbolNode *nodeRes = nullptr;
    if(!symTable) return nullptr;

    uint hash = _symbol_table_hash(symTable, label, labelLen);
    uint index = hash % symTable->tableSize;
    SymbolNode *node = symTable->table[index];

    while(node != nullptr){
        if(node->label){
            if(node->labelLen == labelLen){
                if(StringEqual(label, node->label, labelLen)){
                    nodeRes = node;
                    break;
                }
            }
        }

        node = node->next;
    }

    return nodeRes;
}

SymbolNode *SymbolTable_SymNodeNext(SymbolNode *symNode, char *label, uint len){
    SymbolNode *node = nullptr;
    if(symNode){
        node = symNode->next;
        if(label && len > 0){
            while(node != nullptr){
                if(node->labelLen == len){
                    if(StringEqual(label, node->label, len)){
                        break;
                    }
                }
                node = node->next;
            }
        }
    }
    return node;
}

void SymbolTable_DebugPrint(SymbolTable *symTable){
    if(!symTable) return;
    uint n = symTable->tableSize;
    for(uint i = 0; i < n; i++){
        SymbolNode *node = symTable->table[i];
        if(node){
            if(node->id != TOKEN_ID_NONE && node->label != nullptr){
                printf("[%u] ", i);
                while(node != nullptr){
                    printf("%s (%s)", node->label, Symbol_GetIdString(node->id));

                    node = node->next;
                }

                printf("\n");
            }
        }
    }
}
