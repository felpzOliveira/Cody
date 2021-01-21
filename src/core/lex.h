#pragma once
#include <types.h>
#include <geometry.h>

//I like LEX = Logical EXecutor :)
/* 
  * This is not a real lexer, it a parser that logically process keywords/contexts.
* Writting a regex engine is quite hard and I don't want to use a library,
* so I'm gonna see where parsing can get me.
*/
typedef enum{
    TOKEN_ID_OPERATOR,
    TOKEN_ID_DATATYPE,
    TOKEN_ID_COMMENT,
    TOKEN_ID_STRING,
    TOKEN_ID_NUMBER,
    TOKEN_ID_RESERVED,
    TOKEN_ID_FUNCTION,
    TOKEN_ID_PREPROCESSOR,
    TOKEN_ID_INCLUDE_SEL,
    TOKEN_ID_INCLUDE,
    TOKEN_ID_MATH,
    TOKEN_ID_NONE,
}TokenId;

#define TOKENIZER_OP_FINISHED   0
#define TOKENIZER_OP_UNFINISHED 1
#define TOKENIZER_OP_FAILED     2

#define LEX_PROCESSOR(name) int name(char **p, uint n, char **head, uint *len, TokenizerContext *context)
#define LEX_PROCESSOR_TABLE(name) int name(char **p, uint n, Token *token, TokenLookupTable *lookup)
#define LEX_TOKENIZER_ENTRY_CONTEXT(name) int name(char **p, uint n, TokenizerContext *context)
#define LEX_TOKENIZER_EXEC_CONTEXT(name) int name(char **p, uint n, Token *token, uint *offset, TokenizerContext *context)
#define LEX_TOKENIZER(name) int name(char **p, uint n, Token *token, Tokenizer *tokenizer)


#define LOOKUP_TABLE_INITIALIZER {.table = nullptr, .nSize = 0, .startOffset = 0}
#define TOKENIZER_CONTEXT_INITIALIZER {.entry = nullptr, .lookup = nullptr}
#define TOKENIZER_INITIALIZER {.contexts = nullptr, .contextCount = 0, .unfinishedContext = -1, .linePosition = -1, .lineBeginning = 0}

struct Token;
struct TokenizerContext;

typedef LEX_TOKENIZER_ENTRY_CONTEXT(Lex_ContextEntry);
typedef LEX_TOKENIZER_EXEC_CONTEXT(Lex_ContextExec);

/*
* Basic component for Token information, i.e.: reserved and dynamic generated
* keywords that need to be detected. I'm going to attempt to implement this
* with a growing array and a direct size table.
* 'value' contains the raw string value of the keyword that can be used for matching
* and identifier what class is this 
*/
struct Token{
    int size;
    int position;
    TokenId identifier;
};

struct LookupToken{
    char *value;
    int size;
    TokenId identifier;
};

struct TokenLookupTable{
    LookupToken **table; // the table itself
    int nSize; // the amount of entries in the table
    vec2i *sizes; // amount of elements per entry / reference size of each element
    int startOffset; // the offset that says what is the minimal size of a Token, for C++ this is 2
};

struct TokenizerContext{
    Lex_ContextEntry *entry;
    Lex_ContextExec *exec;
    TokenLookupTable *lookup;
    int is_execing;
    int has_pending_work;
    
    // Comment parsing
    int aggregate;
    int type;
    
    // #include 
    int inclusion;
};

typedef struct{
    Token *workTokenList;
    Token *lastToken;
    uint workTokenListSize;
    uint workTokenListHead;
}TokenizerWorkContext;

struct Tokenizer{
    TokenizerContext *contexts;
    TokenizerWorkContext *workContext;
    int contextCount;
    int unfinishedContext;
    int linePosition;
    int lineBeginning;
};

inline const char *Lex_GetIdString(int id){
#define STR_CASE(x) case x : return #x
    switch(id){
        STR_CASE(TOKEN_ID_OPERATOR);
        STR_CASE(TOKEN_ID_DATATYPE);
        STR_CASE(TOKEN_ID_COMMENT);
        STR_CASE(TOKEN_ID_STRING);
        STR_CASE(TOKEN_ID_NUMBER);
        STR_CASE(TOKEN_ID_PREPROCESSOR);
        STR_CASE(TOKEN_ID_FUNCTION);
        STR_CASE(TOKEN_ID_INCLUDE);
        STR_CASE(TOKEN_ID_MATH);
        STR_CASE(TOKEN_ID_NONE);
        default: return "Invalid";
    }
#undef STR_CASE
}

/* Define few entities for parsing...*/
LEX_PROCESSOR(Lex_Number);
LEX_PROCESSOR(Lex_String);
LEX_PROCESSOR(Lex_Comments);
LEX_PROCESSOR_TABLE(Lex_TokenLookupMatch);
LEX_PROCESSOR_TABLE(Lex_TokenLookupAny);

/* Main call to get tokens, returns the next matching token */
LEX_TOKENIZER(Lex_TokenizeNext);

/* Generic utility for parsing */
#define LEX_PROC_CALLBACK(name) void name(char **p, uint size, uint lineNr, void *prv)
typedef LEX_PROC_CALLBACK(Lex_LineProcessorCallback);

/*
* Parses a text line-by-line giving each line to a processor callback.
*/
void Lex_LineProcess(char *text, uint textsize, Lex_LineProcessorCallback *processor,
                     void *prv=nullptr);

/*
* Builds a tokenizer from default tables.
*/
void Lex_BuildTokenizer(Tokenizer *tokenizer);

/*
* Resets the tokenizer to prepare for a new line of parsing.
*/
void Lex_TokenizerPrepareForNewLine(Tokenizer *tokenizer);