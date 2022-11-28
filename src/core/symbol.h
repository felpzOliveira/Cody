/* date = February 9th 2021 6:34 pm */

#ifndef SYMBOL_H
#define SYMBOL_H
#include <types.h>
#include <geometry.h>
#include <utilities.h>

#define TOKEN_MAX_LENGTH 64
#define SYMBOL_TABLE_SIZE 50000

/*
* We have a few tokens that usually you wouldn't need but these help our
* lexer do its thing and to better work with the symbol table.
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
    TOKEN_ID_COMMENT_TODO,
    TOKEN_ID_COMMENT_NOTE,
    TOKEN_ID_STRING,
    TOKEN_ID_NUMBER,
    TOKEN_ID_RESERVED,
    TOKEN_ID_FUNCTION,
    TOKEN_ID_PREPROCESSOR,
    TOKEN_ID_PREPROCESSOR_DEFINE,
    TOKEN_ID_PREPROCESSOR_IF,
    TOKEN_ID_INCLUDE_SEL,
    TOKEN_ID_INCLUDE,
    TOKEN_ID_MATH,
    TOKEN_ID_SPACE,
    TOKEN_ID_SCOPE, // used only for graphical part to get region from theme
    TOKEN_ID_NONE,

    /* user defined tokens */
    TOKEN_ID_DATATYPE_USER_STRUCT,
    TOKEN_ID_PREPROCESSOR_DEFINITION,
    TOKEN_ID_DATATYPE_USER_DATATYPE,
    TOKEN_ID_DATATYPE_USER_TYPEDEF,
    TOKEN_ID_DATATYPE_USER_ENUM,
    TOKEN_ID_DATATYPE_USER_ENUM_VALUE,
    TOKEN_ID_DATATYPE_USER_CLASS,
    TOKEN_ID_FUNCTION_DECLARATION,
}TokenId;

inline int Symbol_IsTokenJumpable(TokenId id){
    return (!(id == TOKEN_ID_COMMENT || id == TOKEN_ID_STRING));
}

inline int Symbol_IsTokenAutoCompletable(TokenId id){
    return (id != TOKEN_ID_SPACE && id != TOKEN_ID_COMMENT && id != TOKEN_ID_NUMBER &&
            !((int)id >= TOKEN_ID_BRACE_OPEN && (int)id <= TOKEN_ID_ASTERISK) &&
            id != TOKEN_ID_MATH && id != TOKEN_ID_SCOPE);
}

inline int Symbol_IsTokenQueriable(TokenId id){
    if(!((int)id > TOKEN_ID_IGNORE && (int)id < TOKEN_ID_FUNCTION_DECLARATION)){
        return 0;
    }

    return (id != TOKEN_ID_SCOPE && id != TOKEN_ID_SPACE) ? 1 : 0;
}

inline int Symbol_IsTokenNest(TokenId id){
    return (id == TOKEN_ID_PARENTHESE_OPEN || id == TOKEN_ID_PARENTHESE_CLOSE ||
            id == TOKEN_ID_BRACKET_OPEN || id == TOKEN_ID_BRACKET_CLOSE ||
            id == TOKEN_ID_BRACE_OPEN || id == TOKEN_ID_BRACE_CLOSE);
}

inline int Symbol_IsTokenOverriden(TokenId id){
    return (id == TOKEN_ID_NONE || id == TOKEN_ID_FUNCTION || id == TOKEN_ID_FUNCTION_DECLARATION);
}

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
        STR_CASE(TOKEN_ID_RESERVED);
        STR_CASE(TOKEN_ID_COMMA);
        STR_CASE(TOKEN_ID_SEMICOLON);
        STR_CASE(TOKEN_ID_DATATYPE_TYPEDEF_DEF);
        STR_CASE(TOKEN_ID_DATATYPE_STRUCT_DEF);
        STR_CASE(TOKEN_ID_DATATYPE_ENUM_DEF);
        STR_CASE(TOKEN_ID_INCLUDE_SEL);
        STR_CASE(TOKEN_ID_DATATYPE_CLASS_DEF);
        STR_CASE(TOKEN_ID_BRACE_OPEN);
        STR_CASE(TOKEN_ID_BRACE_CLOSE);
        STR_CASE(TOKEN_ID_PARENTHESE_OPEN);
        STR_CASE(TOKEN_ID_PARENTHESE_CLOSE);
        STR_CASE(TOKEN_ID_BRACKET_OPEN);
        STR_CASE(TOKEN_ID_BRACKET_CLOSE);
        STR_CASE(TOKEN_ID_COMMENT);
        STR_CASE(TOKEN_ID_COMMENT_TODO);
        STR_CASE(TOKEN_ID_COMMENT_NOTE);
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
    uint duplications;
    struct symbol_node_t *next;
    struct symbol_node_t *prev;
}SymbolNode;

typedef struct{
    SymbolNode **table;
    uint tableSize;
    uint seed;
    bool allow_duplication;
}SymbolTable;

/*
* Initializes a symbol table. By default the symbol table does not allow
* for duplicated tokens, as even if tokens have the same label they must
* have different meanings given by their ids. You can however make the
* symbol table register how many times a token was inserted by setting
* 'duplicate' = true. Might be usefull if you are using the symbol table
* to keep track of how many times a token appeared.
*/
void SymbolTable_Initialize(SymbolTable *symTable, bool duplicate=false);

/*
* Pushes a new symbol into the symbol table, returns 1 in case the symbol
* was accepted or 0 otherwise.
*/
int SymbolTable_Insert(SymbolTable *symTable, char *label, uint labelLen, TokenId id);

/*
* Removes an entry from the symbol table matching the contents given. If the
* symbol table allows for duplication this routines only removes the entry if
* the token duplication is 0. Otherwise the token duplication value is decreased
* but the entry still persists.
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
* Returns nullptr in case no one is available. Note that because we expect collisions
* it is not garanteed that the next node in the list has the exact same value for label.
* Passing a valid label and n > 0 makes the search looks for a specific entry and not
* the literal 'next' node. Passing label as null or n = 0 makes this routine returns
* the next node.
*/
SymbolNode *SymbolTable_SymNodeNext(SymbolNode *symNode, char *label, uint n);

/*
* Debug routines.
*/
void SymbolTable_DebugPrint(SymbolTable *symTable);

#endif //SYMBOL_H
