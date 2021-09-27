#pragma once
#include <types.h>
#include <geometry.h>
#include <utilities.h>
#include <symbol.h>
#include <languages.h>

//I like LEX = Logical EXecutor :)

/*
* I'm not interested in writing a regex engine. This parser works by tokenizing components
* in the source data, it contains a lookup table that triggers states for logical processing
* each processing unit gets a pointer to LEX_LOGICAL_PROCESSOR, these pointers are added
* to a short stack for continuous updates, at each token if the stack is not empty it
* allows the processing unit to change its state or the token identification based
* on what it thinks it is the correct id, example: the declaration:
*
*      typedef struct lex lex;
* 
* is broken by Lex_TokenizeNext into 8 tokens, i.e.:
* <TOKEN_ID_DATATYPE_TYPEDEF_DEF>, <TOKEN_ID_SPACE>, <TOKEN_ID_DATATYPE_STRUCT_DEF>,
* <TOKEN_ID_SPACE>, <TOKEN_ID_NONE>, <TOKEN_ID_SPACE>, <TOKEN_ID_NONE> and
* <TOKEN_ID_SEMICOLON>.
*
* Because typedef wishes to be a logical processor, it implements LEX_LOGICAL_PROCESSOR
* where it wants to classify the last 'lex' as a <TOKEN_ID_DATATYPE_USER_TYPEDEF>.
* So in its processing unit it looks for states that represent:
*      typedef <SOMETHING> type; and pushes its function into the tokenizer stack.
*
* On the following non empty token <TOKEN_ID_DATATYPE_STRUCT_DEF>, it also wants to
* do logical processing so it can classify tokens as <TOKEN_ID_DATATYPE_USER_STRUCT>
* based on:
*      struct <Anything>{* <Anything> }*
*
* So it pushes its logical processing unit into the stack also. When the next token
* is identified 'lex' (first one), struct's logical unit is called with the token,
* because it is in an unested state and the token is marked as TOKEN_ID_NONE it knows
* this is the name of the struct so it changes its id to <TOKEN_ID_DATATYPE_USER_STRUCT>
* On the following token 'lex' (TOKEN_ID_NONE) once again the struct's logical unit is
* called but now because it already got its key it pops itself and allows the next
* unit to process, i.e.: typedef's. Now typedef's is assure that the <SOMETHING> in its
* rule was consumed and because it is in a unested state it gets the token and marks it
* as <TOKEN_ID_DATATYPE_USER_TYPEDEF>, when the ';' appears, because typedef's unit is 
* in unested state it means the declaration is done. So it pops itself from the stack
* and returns.
* Now we have both 'lex' tokens marked accordingly. I guess in the end this is the
* start of a regexless regex engine.
*/

typedef enum{
    TOKENIZER_STATE_CLEAN,
    TOKENIZER_STATE_MULTILINE,
}TokenizerState;

#define TOKENIZER_OP_FINISHED      0
#define TOKENIZER_OP_UNFINISHED    1
#define TOKENIZER_OP_FAILED        2
#define TOKENIZER_MAX_CACHE_SIZE   64

#define TOKENIZER_FETCH_CALL(name) uint name(char **p, uint n)
typedef TOKENIZER_FETCH_CALL(TokenizerFetchCallback);

#define LEX_PROCESSOR(name) int name(char **p, uint n, char **head, uint *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer)
#define LEX_PROCESSOR_TABLE(name) int name(char **p, uint n, Token *token, TokenLookupTable *lookup, TokenizerFetchCallback *fetcher, TokenizerContext *context, Tokenizer *tokenizer)
#define LEX_TOKENIZER_ENTRY_CONTEXT(name) int name(char **p, uint n, TokenizerContext *context)
#define LEX_TOKENIZER_EXEC_CONTEXT(name) int name(char **p, uint n, Token *token, uint *offset, TokenizerContext *context, Tokenizer *tokenizer)
#define LEX_TOKENIZER(name) int name(char **p, uint n, Token *token, Tokenizer *tokenizer, int process_tab)

#define LEX_LOGICAL_PROCESSOR(name) int name(Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p)


#define TOKEN_INITIALIZER {.size = 0, .position = 0, .identifier = TOKEN_ID_NONE}
#define LOOKUP_TABLE_INITIALIZER {.table = nullptr, .nSize = 0, .startOffset = 0}
#define TOKENIZER_CONTEXT_INITIALIZER {.entry = nullptr, .lookup = nullptr}
#define TOKENIZER_INITIALIZER {.contexts = nullptr, .contextCount = 0, .unfinishedContext = -1, .linePosition = -1, .lineBeginning = 0, .tabSpacing = 1, .fetcher = nullptr, .lastToken = TOKEN_INITIALIZER, .procStack = nullptr }

struct Token;
struct TokenizerContext;
struct Tokenizer;
struct LogicalProcessor;
struct Buffer;

typedef LEX_TOKENIZER_ENTRY_CONTEXT(Lex_ContextEntry);
typedef LEX_TOKENIZER_EXEC_CONTEXT(Lex_ContextExec);
typedef LEX_LOGICAL_PROCESSOR(Lex_LogicalExec);


typedef enum{
    LEX_CONTEXT_ID_PREPROCESSOR,
    LEX_CONTEXT_ID_EXEC_CODE
}ContextID;

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
    void *reserved;
};

/*
* Lookup token is a Token that is only used for constructing the LookupTable
* the Tokenizer will use to match its results.
*/
struct LookupToken{
    char value[TOKEN_MAX_LENGTH];
    int size;
    TokenId identifier;
};

/*
* Table used by Tokenizer to see what it needs to match reserved/special keywords.
*/
struct TokenLookupTable{
    LookupToken **table; // the table itself
    int nSize; // the amount of entries in the table
    int realSize; // the amount of memory for entries in the table
    vec3i *sizes; // amount of elements per entry / reference size of each element / maximum elements per entry
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
* Processor for logical structures.
*/
struct LogicalProcessor{
    Lex_LogicalExec *proc;
    uint nestedLevel;
    uint currentState;
    vec2ui range;
};

/*
* TODO: Port this to the FixedStack?
* Fixed memory stack.
*/
struct BoundedStack{
    LogicalProcessor items[MAX_BOUNDED_STACK_SIZE];
    int capacity;
    int top;
};

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
    BoundedStack procStack;
    
    // utility for faster nest computation and identation
    uint indentLevel;
    uint parenLevel;
    
    // Comment parsing
    int aggregate;
    int type;
    
    // #include 
    int inclusion;
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
    Token lastToken;
    BoundedStack *procStack;
    uint runningLine;
    int givenTokens;
    SymbolTable *symbolTable;
    
    // help bufferview with information about nest at start of line
    uint runningIndentLevel;
    uint runningParenIndentLevel;
    
    // Comment parsing
    int aggregate;
    int type;
    
    // #include 
    int inclusion;

    // support
    TokenizerSupport support;
};

//TODO: I fell like I want to refactor this and write a Token streamer.

/* Define few entities for parsing...*/
LEX_PROCESSOR(Lex_Number);
LEX_PROCESSOR(Lex_String);
LEX_PROCESSOR(Lex_Comments);
LEX_PROCESSOR_TABLE(Lex_TokenLookupMatch);
LEX_PROCESSOR_TABLE(Lex_TokenLookupAny);
LEX_LOGICAL_PROCESSOR(Lex_StructProcessor);
LEX_LOGICAL_PROCESSOR(Lex_TypedefProcessor);
LEX_LOGICAL_PROCESSOR(Lex_EnumProcessor);
LEX_LOGICAL_PROCESSOR(Lex_ClassProcessor);

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
                     uint refLine=0, void *prv=nullptr, bool freeAfter=false);

/*
* Builds a tokenizer from default tables.
*/
void Lex_BuildTokenizer(Tokenizer *tokenizer, int tabSpacing, SymbolTable *symTable,
                        std::vector<std::vector<std::vector<GToken>> *> refTables,
                        TokenizerSupport *support);

/*
* Pushes a new token into the given lookup table.
*/
void Lex_PushTokenIntoTable(TokenLookupTable *lookup, char *value,
                            uint size, TokenId id);

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
void Lex_TokenizerPrepareForNewLine(Tokenizer *tokenizer, uint lineNo);

/*
* Queries the tokenizer in use to report its state _at_this_moment_.
*/
void Lex_TokenizerGetCurrentState(Tokenizer *tokenizer, TokenizerStateContext *context);

/*
* Sets a tokenizer context to be initialized with default values.
*/
void Lex_TokenizerContextEmpty(TokenizerStateContext *context);

/*
* Restore the tokenizer state from a saved context.
*/
void Lex_TokenizerRestoreFromContext(Tokenizer *tokenizer, TokenizerStateContext *context);

/*
* Resets the tokenizer state. Usually you want to call this before
* starting to parse a new file.
*/
void Lex_TokenizerContextReset(Tokenizer *tokenizer);

/*
* Checks if tokenizer has pending work.
*/
int Lex_TokenizerHasPendingWork(Tokenizer *tokenizer);

/*
* Checks if a token is derived from a user definition and not a pre-defined
* table definition.
*/
int Lex_IsUserToken(Token *token);

/* Fixed memory Stack for logical processors */
BoundedStack *BoundedStack_Create();
int BoundedStack_IsFull(BoundedStack *stack);
int BoundedStack_Size(BoundedStack *stack);
int BoundedStack_IsEmpty(BoundedStack *stack);
void BoundedStack_Push(BoundedStack *stack, LogicalProcessor *item);
LogicalProcessor *BoundedStack_Peek(BoundedStack *stack);
void BoundedStack_Pop(BoundedStack *stack);
void BoundedStack_Copy(BoundedStack *dst, BoundedStack *src);
void BoundedStack_SetDefault(BoundedStack *stack);
