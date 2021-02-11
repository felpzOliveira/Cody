#include <symbol.h>
#include <types.h>
#include <utilities.h>

void SymbolTableRow_Initialize(SymbolTableRow *row){
    row->symbols = AllocatorGetN(Symbol, DefaultAllocatorSize);
    row->size = vec3ui(0, DefaultAllocatorSize, 0);
    row->id = TOKEN_ID_IGNORE;
    row->labelLen = 0;
}

void SymbolTable_Initialize(SymbolTable *symTable){
    symTable->table = AllocatorGetN(SymbolTableRow, DefaultAllocatorSize);
    symTable->elements = 0;
    symTable->maxSize = DefaultAllocatorSize;
    for(uint i = 0; i < symTable->maxSize; i++){
        SymbolTableRow_Initialize(&symTable->table[i]);
    }
}

void SymbolTable_Free(SymbolTable *symTable){
    for(uint i = symTable->maxSize-1; i >= 0 ; i--){
        if(symTable->table[i].symbols)
            AllocatorFree(symTable->table[i].symbols);
    }
    
    AllocatorFree(symTable->table);
}

uint SymbolTable_PushSymbol(SymbolTable *symTable, Symbol *symbol, char *label,
                            uint labelLen, TokenId id)
{
    SymbolTableRow *row = nullptr;
    uint is_new = 0;
    uint rid = 0;
    // 1 - Attempt to find a row
    for(uint i = 0; i < symTable->elements; i++){
        SymbolTableRow *r = &symTable->table[i];
        if(r->id == id && r->labelLen == labelLen){
            if(StringEqual((char *)r->label, label, labelLen)){
                row = r;
                rid = i;
                break;
            }
        }
    }
    
    // 2 - If no entry was found create a new one
    if(!row){
        is_new = 1;
        // 2a - check if can borrow a row or need to expand
        if(!(symTable->maxSize > symTable->elements+1)){
            uint newSize = symTable->maxSize + DefaultAllocatorSize;
            symTable->table = AllocatorExpand(SymbolTableRow, symTable->table, newSize);
            for(uint i = symTable->maxSize; i < newSize; i++){
                SymbolTableRow_Initialize(&symTable->table[i]);
            }
            symTable->maxSize = newSize;
        }
        
        rid = symTable->elements;
        row = &symTable->table[symTable->elements];
    }
    
    vec3ui s = row->size;
    uint p = s.z;
    if(s.z > s.y){ // not enough space, expand
        uint n = s.y + DefaultAllocatorSize;
        row->symbols = AllocatorExpand(Symbol, row->symbols, n);
        for(uint i = s.y; i < n; i++){
            row->symbols[i].id = TOKEN_ID_IGNORE;
        }
        
        s = vec3ui(s.x + 1, n, s.x + 1);
        p = row->size.x + 1;
    }else{ // find next empty
        uint found = 0;
        for(uint k = s.z+1; k < s.y; k++){
            if(row->symbols[k].id == TOKEN_ID_IGNORE){
                s.z = k;
                found = 1;
                break;
            }
        }
        
        s.x += 1;
        if(found == 0) s.z = s.y + 1;
    }
    
    // copy token
    row->symbols[p].bufferId = symbol->bufferId;
    row->symbols[p].tokenId =  symbol->tokenId;
    row->symbols[p].id = symbol->id;
    
    if(is_new){ // if new also copy label
        row->labelLen = Min(labelLen, sizeof(row->label));
        Memcpy(row->label, label, row->labelLen);
        row->id = id;
        symTable->elements += 1;
    }
    
    row->size = s;
    return rid;
}

void SymbolTable_DebugPrint(SymbolTable *symTable){
    printf("Symbol table has %u elements\n", symTable->elements);
    printf("Symbol table max size %u\n", symTable->maxSize);
    
    for(uint i = 0; i < symTable->elements; i++){
        SymbolTableRow *row = &symTable->table[i];
        vec3ui size = row->size;
        printf("Row %u ( %u %u %u ) %s %s: ", i, size.x, size.y, size.z,
               Symbol_GetIdString(row->id), row->label);
        
        for(uint k = 0; k < size.x; k++){
            Symbol *s = &row->symbols[k];
            printf(" ( %u %u )  ", s->bufferId, s->tokenId);
        }
        
        printf("\n");
    }
}