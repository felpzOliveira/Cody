/* date = February 9th 2021 6:34 pm */

#ifndef SYMBOL_H
#define SYMBOL_H
#include <types.h>
#include <geometry.h>

#define TOKEN_MAX_LENGTH 64

typedef enum{
    TOKEN_ID_IGNORE = 0,
    TOKEN_ID_OPERATOR,
    TOKEN_ID_DATATYPE,
    TOKEN_ID_BRACE_OPEN,
    TOKEN_ID_BRACE_CLOSE,
    TOKEN_ID_PARENTHESE_OPEN,
    TOKEN_ID_PARENTHESE_CLOSE,
    TOKEN_ID_BRACKET_OPEN,
    TOKEN_ID_BRACKET_CLOSE,
    TOKEN_ID_MORE,
    TOKEN_ID_LESS,
    TOKEN_ID_COMMA,
    TOKEN_ID_SEMICOLON,
    TOKEN_ID_ASTERISK,
    TOKEN_ID_DATATYPE_STRUCT_DEF,
    TOKEN_ID_DATATYPE_TYPEDEF_DEF,
    TOKEN_ID_DATATYPE_ENUM_DEF,
    TOKEN_ID_DATATYPE_CLASS_DEF,
    TOKEN_ID_COMMENT,
    TOKEN_ID_STRING,
    TOKEN_ID_NUMBER,
    TOKEN_ID_RESERVED,
    TOKEN_ID_FUNCTION,
    TOKEN_ID_PREPROCESSOR,
    TOKEN_ID_PREPROCESSOR_DEFINE,
    TOKEN_ID_PREPROCESSOR_DEFINITION,
    TOKEN_ID_INCLUDE_SEL,
    TOKEN_ID_INCLUDE,
    TOKEN_ID_MATH,
    TOKEN_ID_SPACE,
    TOKEN_ID_NONE,
    
    /* user defined tokens */
    TOKEN_ID_DATATYPE_USER_STRUCT,
    TOKEN_ID_DATATYPE_USER_TYPEDEF,
    TOKEN_ID_DATATYPE_USER_ENUM,
    TOKEN_ID_DATATYPE_USER_CLASS
}TokenId;

inline int Symbol_AreTokensComplementary(TokenId id0, TokenId id1){
    return ((id0 == TOKEN_ID_PARENTHESE_OPEN && id1 == TOKEN_ID_PARENTHESE_CLOSE) ||
            (id0 == TOKEN_ID_PARENTHESE_CLOSE && id1 == TOKEN_ID_PARENTHESE_OPEN) ||
            (id0 == TOKEN_ID_BRACKET_OPEN && id1 == TOKEN_ID_BRACKET_CLOSE) ||
            (id0 == TOKEN_ID_BRACKET_CLOSE && id1 == TOKEN_ID_BRACKET_OPEN) ||
            (id0 == TOKEN_ID_BRACE_OPEN && id1 == TOKEN_ID_BRACE_CLOSE) ||
            (id0 == TOKEN_ID_BRACE_CLOSE && id1 == TOKEN_ID_BRACE_OPEN));
}

inline const char *Symbol_GetIdString(int id){
#define STR_CASE(x) case x : return #x
    switch(id){
        STR_CASE(TOKEN_ID_OPERATOR);
        STR_CASE(TOKEN_ID_DATATYPE);
        STR_CASE(TOKEN_ID_COMMA);
        STR_CASE(TOKEN_ID_SEMICOLON);
        STR_CASE(TOKEN_ID_DATATYPE_TYPEDEF_DEF);
        STR_CASE(TOKEN_ID_DATATYPE_STRUCT_DEF);
        STR_CASE(TOKEN_ID_DATATYPE_ENUM_DEF);
        STR_CASE(TOKEN_ID_DATATYPE_CLASS_DEF);
        STR_CASE(TOKEN_ID_BRACE_OPEN);
        STR_CASE(TOKEN_ID_BRACE_CLOSE);
        STR_CASE(TOKEN_ID_PARENTHESE_OPEN);
        STR_CASE(TOKEN_ID_PARENTHESE_CLOSE);
        STR_CASE(TOKEN_ID_BRACKET_OPEN);
        STR_CASE(TOKEN_ID_BRACKET_CLOSE);
        STR_CASE(TOKEN_ID_COMMENT);
        STR_CASE(TOKEN_ID_STRING);
        STR_CASE(TOKEN_ID_NUMBER);
        STR_CASE(TOKEN_ID_PREPROCESSOR);
        STR_CASE(TOKEN_ID_PREPROCESSOR_DEFINE);
        STR_CASE(TOKEN_ID_PREPROCESSOR_DEFINITION);
        STR_CASE(TOKEN_ID_FUNCTION);
        STR_CASE(TOKEN_ID_INCLUDE);
        STR_CASE(TOKEN_ID_MATH);
        STR_CASE(TOKEN_ID_ASTERISK);
        STR_CASE(TOKEN_ID_NONE);
        STR_CASE(TOKEN_ID_SPACE);
        STR_CASE(TOKEN_ID_MORE);
        STR_CASE(TOKEN_ID_LESS);
        STR_CASE(TOKEN_ID_DATATYPE_USER_STRUCT);
        STR_CASE(TOKEN_ID_DATATYPE_USER_TYPEDEF);
        STR_CASE(TOKEN_ID_DATATYPE_USER_ENUM);
        STR_CASE(TOKEN_ID_DATATYPE_USER_CLASS);
        default: return "Invalid";
    }
#undef STR_CASE
}

typedef struct{
    uint bufferId;
    uint tokenId;
    TokenId id;
}Symbol;

/*
* Size is:
*         x - number of current elements;
*         y - maximum size of the symbol array;
*         z - position of the first clean element;
*/
typedef struct{
    Symbol *symbols;
    vec3ui size;
    TokenId id;
    char label[TOKEN_MAX_LENGTH];
    uint labelLen;
}SymbolTableRow;

typedef struct{
    SymbolTableRow *table;
    uint elements;
    uint maxSize;
}SymbolTable;

/*
* Initializes a symbol table.
*/
void SymbolTable_Initialize(SymbolTable *symTable);

/*
* Releases the memory related to a symbol table.
*/
void SymbolTable_Free(SymbolTable *symTable);

/*
* Pushes a new entry into the symbol table. Returns the row id inserted.
*/
uint SymbolTable_PushSymbol(SymbolTable *symTable, Symbol *symbol, char *label,
                            uint labelLen, TokenId id);

/* Debug stuff */
void SymbolTable_DebugPrint(SymbolTable *symTable);

#endif //SYMBOL_H
