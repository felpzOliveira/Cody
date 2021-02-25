/* date = February 9th 2021 6:34 pm */

#ifndef SYMBOL_H
#define SYMBOL_H
#include <types.h>
#include <geometry.h>

#define TOKEN_MAX_LENGTH 64
#define SYMBOL_TABLE_SIZE 50000

/*
* We have a few tokens that usually you wouldn't need but these help our
* lexer to its thing and to better work with the symbol table.
*/
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
    TOKEN_ID_FUNCTION_DECLARATION,
    TOKEN_ID_PREPROCESSOR,
    TOKEN_ID_PREPROCESSOR_DEFINE,
    TOKEN_ID_PREPROCESSOR_IF,
    TOKEN_ID_PREPROCESSOR_DEFINITION,
    TOKEN_ID_INCLUDE_SEL,
    TOKEN_ID_INCLUDE,
    TOKEN_ID_MATH,
    TOKEN_ID_SPACE,
    TOKEN_ID_SCOPE, // used only for graphical part to get region from theme
    TOKEN_ID_NONE,
    
    /* user defined tokens */
    TOKEN_ID_DATATYPE_USER_STRUCT,
    TOKEN_ID_DATATYPE_USER_DATATYPE,
    TOKEN_ID_DATATYPE_USER_TYPEDEF,
    TOKEN_ID_DATATYPE_USER_ENUM,
    TOKEN_ID_DATATYPE_USER_ENUM_VALUE,
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
        STR_CASE(TOKEN_ID_PREPROCESSOR_IF);
        STR_CASE(TOKEN_ID_PREPROCESSOR_DEFINITION);
        STR_CASE(TOKEN_ID_FUNCTION);
        STR_CASE(TOKEN_ID_FUNCTION_DECLARATION);
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
        STR_CASE(TOKEN_ID_DATATYPE_USER_ENUM_VALUE);
        STR_CASE(TOKEN_ID_DATATYPE_USER_CLASS);
        STR_CASE(TOKEN_ID_DATATYPE_USER_DATATYPE);
        default: return "Invalid";
    }
#undef STR_CASE
}

typedef struct symbol_node_t{
    char *label;
    uint labelLen;
    TokenId id;
    struct symbol_node_t *next;
    struct symbol_node_t *prev;
}SymbolNode;

typedef struct{
    SymbolNode **table;
    uint tableSize;
    uint seed;
}SymbolTable;

/*
* Initializes a symbol table.
*/
void SymbolTable_Initialize(SymbolTable *symTable);

/*
* Pushes a new symbol into the symbol table.
*/
void SymbolTable_Insert(SymbolTable *symTable, char *label, uint labelLen, TokenId id);

/*
* Removes an entry from the symbol table matching the contents and location.
*/
void SymbolTable_Remove(SymbolTable *symTable, char *label, uint labelLen, TokenId id);

/*
* Queries a symbol table for the first matching hashed node.
*/
SymbolNode *SymbolTable_Search(SymbolTable *symTable, char *label, uint labelLen);

/*
* Queries a symbol table for a specific entry matching the input data.
*/
SymbolNode *SymbolTable_GetEntry(SymbolTable *symTable, char *label, uint labelLen,
                                 TokenId id, uint *tableIndex);

/*
* Gets the next symbol in the symbol table given a specific already hashed symbol.
* Returns nullptr in case no one is available.
*/
SymbolNode *SymbolTable_SymNodeNext(SymbolNode *symNode);

/*
* Debug routines.
*/
void SymbolTable_DebugPrint(SymbolTable *symTable);

#endif //SYMBOL_H
