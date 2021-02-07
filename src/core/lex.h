#pragma once
#include <types.h>
#include <geometry.h>

//I like LEX = Logical EXecutor :)
/* 
  * This is not a real lexer, it is a parser that logically process keywords/contexts.
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
    TOKEN_ID_SPACE,
    TOKEN_ID_NONE,
}TokenId;

typedef enum{
    TOKENIZER_STATE_CLEAN,
    TOKENIZER_STATE_MULTILINE,
}TokenizerState;

#define TOKENIZER_OP_FINISHED   0
#define TOKENIZER_OP_UNFINISHED 1
#define TOKENIZER_OP_FAILED     2

#define TOKENIZER_FETCH_CALL(name) uint name(char **p, uint n)
typedef TOKENIZER_FETCH_CALL(TokenizerFetchCallback);

#define LEX_PROCESSOR(name) int name(char **p, uint n, char **head, uint *len, TokenizerContext *context)
#define LEX_PROCESSOR_TABLE(name) int name(char **p, uint n, Token *token, TokenLookupTable *lookup, TokenizerFetchCallback *fetcher)
#define LEX_TOKENIZER_ENTRY_CONTEXT(name) int name(char **p, uint n, TokenizerContext *context)
#define LEX_TOKENIZER_EXEC_CONTEXT(name) int name(char **p, uint n, Token *token, uint *offset, TokenizerContext *context, TokenizerFetchCallback *fetcher)
#define LEX_TOKENIZER(name) int name(char **p, uint n, Token *token, Tokenizer *tokenizer)


#define LOOKUP_TABLE_INITIALIZER {.table = nullptr, .nSize = 0, .startOffset = 0}
#define TOKENIZER_CONTEXT_INITIALIZER {.entry = nullptr, .lookup = nullptr}
#define TOKENIZER_INITIALIZER {.contexts = nullptr, .contextCount = 0, .unfinishedContext = -1, .linePosition = -1, .lineBeginning = 0, .tabSpacing = 1, .fetcher = nullptr}
#define TOKENIZER_STATE_INITIALIZER {.state = TOKENIZER_STATE_CLEAN, .activeWorkProcessor = -1, .backTrack = 0, .forwardTrack = 0}

struct Token;
struct TokenizerContext;

typedef LEX_TOKENIZER_ENTRY_CONTEXT(Lex_ContextEntry);
typedef LEX_TOKENIZER_EXEC_CONTEXT(Lex_ContextExec);

/*
* Basic component for Token information, i.e.: reserved and dynamic generated
* keywords that need to be detected. I'm going to attempt to implement this
* with a growing array and a direct size table. Token is represented by its size,
* starting position in the line that generated it and a identifier so we can check
* what it represents.
*/
struct Token{
    int size;
    int position;
    TokenId identifier;
};

/*
* Lookup token is a Token that is only used for constructing the LookupTable
* the Tokenizer will use to match its results.
*/
struct LookupToken{
    char *value;
    int size;
    TokenId identifier;
};

/*
* Table used by Tokenizer to see what it needs to match reserved/special keywords.
*/
struct TokenLookupTable{
    LookupToken **table; // the table itself
    int nSize; // the amount of entries in the table
    vec2i *sizes; // amount of elements per entry / reference size of each element
    int startOffset; // the offset that says what is the minimal size of a Token, for C++ this is 2
};

/*
* Tokenizer context, 'entry' must specify if given a line the context
 * wishes to be invoked for token generation, 'exec' asks the context to parse
* 1 token that it can accept and return the length of this token. 'lookup'
* is a storage reserved for a table if the context requires one.
*/
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

/*
* Utility used by the parsing routines to accelerate tokenization.
* Tokens get generated inside a TokenizerWorkContext that caches
* work arrays for faster copy/insertion.
*/
typedef struct{
    Token *workTokenList;
    Token *lastToken;
    uint workTokenListSize;
    uint workTokenListHead;
}TokenizerWorkContext;

/*
* Tokenizer state. Can be used to save and restore tokenization from
* a specific point. While 'Lex_TokenizerGetCurrentState' can retrieve most
* of these, 'forwardTrack' needs to be explicitly defined by querying the
* 'backTrack' property of the previously processed line. 
*/
typedef struct{
    TokenizerState state;
    int activeWorkProcessor;
    uint backTrack;
    uint forwardTrack;
}TokenizerStateContext;

/*
* The actual Tokenizer structure. Contains a list of contexts that need to run,
 * a work context for faster token insertion, and a few utilities for querying
* its state and pending works.
*/
struct Tokenizer{
    TokenizerContext *contexts;
    TokenizerWorkContext *workContext;
    int contextCount;
    int unfinishedContext;
    int linesAggregated;
    int linePosition;
    int lineBeginning;
    int autoIncrementor;
    int tabSpacing;
    TokenizerFetchCallback *fetcher;
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
#define LEX_PROC_CALLBACK(name) void name(char **p, uint size, uint lineNr, uint at, uint total, void *prv)
typedef LEX_PROC_CALLBACK(Lex_LineProcessorCallback);


/*
* Parses a text line-by-line giving each line to a processor callback.
* The line when given has '\n' traded by 0, and size+1.
*/
void Lex_LineProcess(char *text, uint textsize, Lex_LineProcessorCallback *processor,
                     void *prv=nullptr);

/*
* Builds a tokenizer from default tables.
*/
void Lex_BuildTokenizer(Tokenizer *tokenizer, int tabSpacing);

/*
* Sets the tokenizer fetcher call for Tokens that cannot be determined by the 
* current state of the line. The fetcher callback must be able to retrieve a segment of 
* text that follows or inform that there is none available, in which case the
* Token is marked as TOKEN_ID_NONE.
*/
void Lex_TokenizerSetFetchCallback(Tokenizer *tokenizer,
                                   TokenizerFetchCallback *callback);

/*
* Resets the tokenizer to prepare for a new line of parsing.
*/
void Lex_TokenizerPrepareForNewLine(Tokenizer *tokenizer);

/*
* Basically 'Lex_TokenizerPrepareForNewLine' but also resets
* the state of the previous line processed.
*/
void Lex_TokenizerReset(Tokenizer *tokenizer);

/*
* Queries the tokenizer in use to report its state _at_this_moment_.
*/
void Lex_TokenizerGetCurrentState(Tokenizer *tokenizer, TokenizerStateContext *context);

/*
* Restore the tokenizer state from a saved context.
*/
void Lex_TokenizerRestoreFromContext(Tokenizer *tokenizer, TokenizerStateContext *context);

/*
* Checks if tokenizer has pending work.
*/
int Lex_TokenizerHasPendingWork(Tokenizer *tokenizer);