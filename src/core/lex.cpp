#include <stdio.h>
#include <string.h>
#include <lex.h>
#include <sstream>
#include <utilities.h>
#include <vector>
#include <buffers.h>

#define MODULE_NAME "Lex"

#define INC_OR_ZERO(p, n, r) do{ if((r) < n) (*p)++; else return 0; }while(0)

inline void TokenizerCacheSym(uint id, Tokenizer *tokenizer){
    AssertErr(tokenizer->commitedSymsCount+1 < TOKENIZER_MAX_CACHE_SIZE,
              "Tokenizer cache is out of memory");
    tokenizer->commitedSyms[tokenizer->commitedSymsCount++] = id;
}

inline void TokenizerCommitCache(Tokenizer *tokenizer){
    if(tokenizer->trackSymbols){
        for(uint i = 0; i < tokenizer->commitedSymsCount; i++){
            uint rid = tokenizer->commitedSyms[i];
            SymbolTableRow *row = &tokenizer->symbolTable.table[rid];
            for(int i = 0; i < tokenizer->contextCount; i++){
                Lex_PushTokenIntoTable(tokenizer->contexts[i].lookup, row->label,
                                       row->labelLen, row->id);
            }
        }
        
        tokenizer->commitedSymsCount = 0;
    }
}

inline void TokenizerConsumeCacheIfPossible(Tokenizer *tokenizer){
    if(BoundedStack_IsEmpty(tokenizer->procStack)){
        TokenizerCommitCache(tokenizer);
    }
}

inline void TokenizerUpdateState(Tokenizer *tokenizer, Token *token){
    if(token->identifier == TOKEN_ID_BRACE_OPEN){
        tokenizer->runningIndentLevel++;
    }else if(token->identifier == TOKEN_ID_BRACE_CLOSE){
        uint n = tokenizer->runningIndentLevel;
        tokenizer->runningIndentLevel = n > 0 ? n - 1 : 0;
    }else if(token->identifier == TOKEN_ID_PARENTHESE_OPEN){
        tokenizer->runningParenIndentLevel++;
    }else if(token->identifier == TOKEN_ID_PARENTHESE_CLOSE){
        uint n = tokenizer->runningParenIndentLevel;
        tokenizer->runningParenIndentLevel = n > 0 ? n - 1 : 0;
    }
}

inline int Lex_IsTokenReserved(Token *token){
    TokenId id = token->identifier;
    return (id == TOKEN_ID_RESERVED ||
            id == TOKEN_ID_PREPROCESSOR || id == TOKEN_ID_PREPROCESSOR_DEFINE ||
            id == TOKEN_ID_PREPROCESSOR_DEFINITION || id == TOKEN_ID_INCLUDE_SEL ||
            id == TOKEN_ID_INCLUDE);
}

inline int Lex_IsHexValue(char v){
    return (v >= 'A' && v <= 'F') || (v >= 'a' && v <= 'f');
}

inline int Lex_IsDecimalValue(char v){
    int s = v - '0';
    return (s >= 0 && s <= 9);
}

inline int Lex_IsAnyNumber(char v){
    return Lex_IsDecimalValue(v) || Lex_IsHexValue(v);
}

static char Lex_LookAhead(char *p, uint start, uint maxn, TokenizerFetchCallback *fetcher)
{
    char *s = p;
    uint runLen = maxn;
    uint n = 0;
    uint fetched = start;
    while(s != NULL){
        while((*s == ' ' || *s == '\n' || *s == '\t' || *s == '\r') && n < runLen){
            s++;
            n++;
        }
        
        if(n < runLen) return *s;
        
        // Need to look next segment
        if(fetcher){
            fetched += n;
            runLen = fetcher(&s, fetched);
        }else{
            s = NULL;
        }
    }
    
    return 0;
}

inline int Lex_IsUserToken(Token *token){
    return (token->identifier >= TOKEN_ID_DATATYPE_USER_STRUCT ? 1 : 0);
}

static int Lex_ProcStackInsert(Tokenizer *tokenizer, Token *token){
    int added = 0;
    if(token->identifier == TOKEN_ID_DATATYPE_STRUCT_DEF){
        LogicalProcessor proc = {
            .proc = Lex_StructProcessor,
            .nestedLevel = 0,
            .currentState = 0,
        };
        
        BoundedStack_Push(tokenizer->procStack, &proc);
        added = 1;
    }else if(token->identifier == TOKEN_ID_DATATYPE_TYPEDEF_DEF){
        LogicalProcessor proc = {
            .proc = Lex_TypedefProcessor,
            .nestedLevel = 0,
            .currentState = 0,
        };
        
        BoundedStack_Push(tokenizer->procStack, &proc);
        added = 1;
    }else if(token->identifier == TOKEN_ID_DATATYPE_ENUM_DEF){
        LogicalProcessor proc = {
            .proc = Lex_EnumProcessor,
            .nestedLevel = 0,
            .currentState = 0,
        };
        
        BoundedStack_Push(tokenizer->procStack, &proc);
        added = 1;
    }else if(token->identifier == TOKEN_ID_DATATYPE_CLASS_DEF){
        LogicalProcessor proc = {
            .proc = Lex_ClassProcessor,
            .nestedLevel = 0,
            .currentState = 0,
        };
        
        BoundedStack_Push(tokenizer->procStack, &proc);
        added = 1;
    }
    
    return added;
}

int Lex_TokenLogicalFilter(Tokenizer *tokenizer, Token *token, 
                           LogicalProcessor *proc, TokenId refId, char **p)
{
    if(token->identifier == refId) return 1;
    int filtered = 1;
    switch(token->identifier){
        /* templates */
        case TOKEN_ID_LESS: proc->nestedLevel++; break;
        case TOKEN_ID_MORE: proc->nestedLevel > 0 ? proc->nestedLevel-- : 0; break;
        /* definition */
        case TOKEN_ID_BRACE_OPEN: proc->nestedLevel++; break;
        case TOKEN_ID_BRACE_CLOSE: proc->nestedLevel > 0 ? proc->nestedLevel-- : 0; break;
        /* function */
        case TOKEN_ID_PARENTHESE_OPEN: proc->nestedLevel++; break;
        case TOKEN_ID_PARENTHESE_CLOSE: proc->nestedLevel > 0 ? proc->nestedLevel-- : 0; break;
        /* possible end of declaration */
        case TOKEN_ID_SEMICOLON:{
            if(proc->nestedLevel == 0){ // finished declaration
                BoundedStack_Pop(tokenizer->procStack);
                filtered = 2;
            }
        } break;
        default: filtered = 0; break;
    }
    
    //TODO: This tests avoid broken statements to propagate
    //      the logical expression to the hole doc. Check if it is sane.
    if(Lex_IsTokenReserved(token)){
        BoundedStack_Pop(tokenizer->procStack);
        filtered = 1;
    }
    
    return filtered;
}

/* (Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p) */
LEX_LOGICAL_PROCESSOR(Lex_ClassProcessor){
    /*
* class A
*/
    int emitWarningToken = 0;
    int filter = Lex_TokenLogicalFilter(tokenizer, token, proc,
                                        TOKEN_ID_DATATYPE_CLASS_DEF, p);
    
    char *h = (*p) - token->size;
    int grabbed = 0;
    switch(token->identifier){
        case TOKEN_ID_BRACE_CLOSE:{
            if(proc->nestedLevel == 0){
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 1;
                filter = 2;
            }
        } break;
        /* possible token */
        case TOKEN_ID_NONE:{
            if(proc->nestedLevel == 0 && proc->currentState == 0){
                token->identifier = TOKEN_ID_DATATYPE_USER_CLASS;
                proc->currentState = 1;
                grabbed = 1;
            }else if(proc->nestedLevel == 0 && proc->currentState == 1){
                /*
* This means we already got our token but there is another
* this is most likely a construction of either invalid struct
* or a forward declaration.
*/
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 2;
                filter = 2;
            }
        } break;
        
        default: {}
    }
    
    if(emitWarningToken){
        if(!BoundedStack_IsEmpty(tokenizer->procStack)){
            Token infoToken;
            infoToken.identifier = TOKEN_ID_IGNORE;
            LogicalProcessor *nextProc = BoundedStack_Peek(tokenizer->procStack);
            nextProc->proc(tokenizer, &infoToken, nextProc, p);
            if(emitWarningToken == 2){ // give real token also
                nextProc->proc(tokenizer, token, nextProc, p);
            }
        }
    }
    
    if(grabbed && tokenizer->trackSymbols){
        Symbol s = {
            .bufferId = tokenizer->runningLine,
            .tokenId = (uint)tokenizer->givenTokens,
            .id = token->identifier,
        };
        
        uint rid = SymbolTable_PushSymbol(&tokenizer->symbolTable, &s, h, token->size,
                                          token->identifier);
        
        TokenizerCacheSym(rid, tokenizer);
    }
    
    return filter > 1 ? 0 : 1;
}

/* (Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p) */
LEX_LOGICAL_PROCESSOR(Lex_EnumProcessor){
    /*
* enum A{ <Anything> };
* 
*/
    int emitWarningToken = 0;
    int filter = Lex_TokenLogicalFilter(tokenizer, token, proc,
                                        TOKEN_ID_DATATYPE_STRUCT_DEF, p);
    switch(token->identifier){
        case TOKEN_ID_BRACE_CLOSE:{
            if(proc->nestedLevel == 0){
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 1;
                filter = 2;
            }
        } break;
        /* possible token */
        case TOKEN_ID_NONE:{
            if(proc->nestedLevel == 0 && proc->currentState == 0){
                token->identifier = TOKEN_ID_DATATYPE_USER_ENUM;
                proc->currentState = 1;
            }else if(proc->nestedLevel == 0 && proc->currentState == 1){
                /*
* This means we already got our token but there is another
* this is most likely a construction of either invalid struct
* or a forward declaration.
*/
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 2;
                filter = 2;
            }
        } break;
        
        default: {}
    }
    
    if(emitWarningToken){
        if(!BoundedStack_IsEmpty(tokenizer->procStack)){
            Token infoToken;
            infoToken.identifier = TOKEN_ID_IGNORE;
            LogicalProcessor *nextProc = BoundedStack_Peek(tokenizer->procStack);
            nextProc->proc(tokenizer, &infoToken, nextProc, p);
            if(emitWarningToken == 2){ // give real token also
                nextProc->proc(tokenizer, token, nextProc, p);
            }
        }
    }
    
    return filter > 1 ? 0 : 1;
}

/* (Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p) */
LEX_LOGICAL_PROCESSOR(Lex_StructProcessor){
    /*
* struct A;
* struct A{ <Anything> };
*/
    int emitWarningToken = 0;
    int filter = Lex_TokenLogicalFilter(tokenizer, token, proc,
                                        TOKEN_ID_DATATYPE_STRUCT_DEF, p);
    
    char *h = (*p) - token->size;
    int grabbed = 0;
    switch(token->identifier){
        case TOKEN_ID_BRACE_CLOSE:{
            if(proc->nestedLevel == 0){
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 1;
                filter = 2;
            }
        } break;
        /* possible token */
        case TOKEN_ID_NONE:{
            if(proc->nestedLevel == 0 && proc->currentState == 0){
                token->identifier = TOKEN_ID_DATATYPE_USER_STRUCT;
                proc->currentState = 1;
                grabbed = 1;
            }else if(proc->nestedLevel == 0 && proc->currentState == 1){
                /*
* This means we already got our token but there is another
* this is most likely a constr  uction of either invalid struct
* or a forward declaration.
*/
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 2;
                filter = 2;
            }
        } break;
        
        default: {}
    }
    
    if(emitWarningToken){
        if(!BoundedStack_IsEmpty(tokenizer->procStack)){
            Token infoToken;
            infoToken.identifier = TOKEN_ID_IGNORE;
            LogicalProcessor *nextProc = BoundedStack_Peek(tokenizer->procStack);
            nextProc->proc(tokenizer, &infoToken, nextProc, p);
            if(emitWarningToken == 2){ // give real token also
                nextProc->proc(tokenizer, token, nextProc, p);
            }
        }
    }
    
    if(grabbed && tokenizer->trackSymbols){
        Symbol s = {
            .bufferId = tokenizer->runningLine,
            .tokenId = (uint)tokenizer->givenTokens,
            .id = token->identifier,
        };
        uint rid = SymbolTable_PushSymbol(&tokenizer->symbolTable, &s, h, token->size,
                                          token->identifier);
        TokenizerCacheSym(rid, tokenizer);
    }
    
    
    return filter > 1 ? 0 : 1;
}

/* (Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p) */
LEX_LOGICAL_PROCESSOR(Lex_TypedefProcessor){
    //TODO: This needs to be seriously reviewd;
    /*
* Very soft representation of typedef as function pointers are quite annoying.
*
* typedef <SOMETHING> A;
* typedef <SOMETHING> A, B, *C;
* typedef int (*myFuncDef)(int b, int a);
*/
    Token *lastToken = &tokenizer->lastToken;
    int filter = Lex_TokenLogicalFilter(tokenizer, token, proc,
                                        TOKEN_ID_DATATYPE_TYPEDEF_DEF, p);
    int grabbed = 0;
    char *h = (*p) - token->size;
    
    if(token->identifier != TOKEN_ID_DATATYPE_TYPEDEF_DEF){
        if(token->identifier == TOKEN_ID_PARENTHESE_OPEN){
            if(proc->nestedLevel == 1){
                proc->currentState = 2;
            }
        }else if(proc->currentState == 2){
            if(token->identifier == TOKEN_ID_ASTERISK && 
               lastToken->identifier == TOKEN_ID_PARENTHESE_OPEN)
            {
                proc->currentState = 3;
            }
        }else if(proc->currentState == 3 && token->identifier == TOKEN_ID_NONE){
            //token->identifier = TOKEN_ID_DATATYPE_USER_TYPEDEF;
            token->identifier = TOKEN_ID_DATATYPE_USER_DATATYPE;
            grabbed = 1;
        }else if(token->identifier == TOKEN_ID_NONE && 
                 proc->nestedLevel == 0 && proc->currentState == 1)
        {
            //token->identifier = TOKEN_ID_DATATYPE_USER_TYPEDEF;
            token->identifier = TOKEN_ID_DATATYPE_USER_DATATYPE;
            grabbed = 1;
        }else if(proc->currentState == 2 && token->identifier == TOKEN_ID_PARENTHESE_CLOSE){
            proc->currentState = 1;
        }else if(proc->nestedLevel == 0 && proc->currentState == 0){
            proc->currentState = 1;
        }
    }
    
    if(token->identifier == TOKEN_ID_IGNORE){
        proc->currentState = 1;
    }
    
    if(grabbed && tokenizer->trackSymbols){
        Symbol s = {
            .bufferId = tokenizer->runningLine,
            .tokenId = (uint)tokenizer->givenTokens,
            .id = token->identifier,
        };
        
        uint rid = SymbolTable_PushSymbol(&tokenizer->symbolTable, &s, h, token->size,
                                          token->identifier);
        TokenizerCacheSym(rid, tokenizer);
    }
    
    return filter > 1 ? 0 : 1;
}

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer) */
LEX_PROCESSOR(Lex_Number){
    (void)context;
    int iNum = 0;
    int dotCount = 0;
    int eCount = 0;
    int xCount = 0;
    int rLen = 0;
    int iValue = 0;
    int exValue = 0;
    *len = 0;
    *head = *p;
    
    LEX_DEBUG("Starting Numeric parser at \'%c\'\n", **p);
    
    iValue = (**p) - '0';
    
    if(**p == '.'){
        if(n < 2) return 0;
        int k = *((*p)+1) - '0';
        if(k < 0 || k > 9) return 0;
        
        dotCount = 1;
        rLen = 1;
        INC_OR_ZERO(p, n, rLen);
    }
    
    if(dotCount == 0 && (iValue < 0 || iValue > 9)) return 0;
    
    do{
        _number_start:
        iValue = (**p) - '0';
        exValue = (int)(**p);
        if(**p == '.'){
            dotCount ++;
            if(dotCount > 1){
                LEX_DEBUG("Multiple dot values\n");
                *len = rLen;
                return 2;
            }
            
            rLen ++;
            (*p)++;
            goto _number_start;
        }else if(**p == 'f' && xCount == 0){
            if(dotCount < 1){
                LEX_DEBUG("\'f\' found without \'.\'\n");
                goto end;
            }
            
            rLen++;
            (*p)++;
            goto end;
        }else if(**p == 'x' || **p == 'X'){
            if(rLen > 1){
                LEX_DEBUG("Found \'x\' but token is large\n");
                goto end;
            }
            
            if(xCount != 0){
                LEX_DEBUG("Found multiple \'x\'\n");
                goto end;
            }
            
            if(rLen+1 < n){
                if(Lex_IsAnyNumber((*((*p)+1))) && dotCount == 0 && eCount == 0){
                    xCount = 1;
                    rLen++;
                    (*p)++;
                    goto _number_start;
                }
            }else{
                goto end;
            }
        }else if(**p == 'e' || **p == 'E'){
            if(rLen < 1){
                LEX_DEBUG("Found \'e\' at first token position\n");
                return 0;
            }
            
            eCount++;
            if(eCount > 1){
                LEX_DEBUG("Multiple \'e\' found in token\n");
                goto end;
            }
            
            if(xCount > 0){
                LEX_DEBUG("Found \'e\' but number has \'x\' in token\n");
                goto end;
            }
            
            // check for symbol and/or value
            if(rLen+1 < n){
                int sValue = *((*p)+1) - '0';
                if(*((*p)+1) == '+' || *((*p)+1) == '-'){
                    // move to next symbol
                    if(!(rLen+2 < n)){
                        LEX_DEBUG("Found \'e(+-)\' wihtout space\n");
                        goto end;
                    }
                    sValue = *((*p)+2) - '0';
                    if(!(sValue >= 0 && sValue <= 9)){
                        LEX_DEBUG("Found \'e(+-)\' without value\n");
                        goto end;
                    }
                    
                    rLen += 2;
                    (*p)++; (*p)++;
                    goto _number_start;
                }else if((sValue >= 0 && sValue <= 9)){
                    rLen++;
                    (*p)++;
                    goto _number_start;
                }else{
                    LEX_DEBUG("Found \'e\' but no value\n");
                    goto end;
                }
            }else{
                LEX_DEBUG("Found \'e\' without space left\n");
                goto end;
            }
        }
        
        iNum = Lex_IsDecimalValue(**p);
        if(!iNum && xCount == 1){
            iNum = Lex_IsHexValue(**p);
        }
        
        if(iNum){
            rLen ++;
            (*p)++;
        }
    }while(iNum != 0 && rLen < n);
    
    end:
    *len = rLen;
    return rLen > 0 ? 1 : 0;
}

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer) */
LEX_PROCESSOR(Lex_Inclusion){
    int level = 0;
    int rLen = 0;
    int reverseSlash = 0;
    *len = 0;
    *head = *p;
    LEX_DEBUG("Starting inclusion parser at \'%c\'\n", **p);
    
    if(**p == '<'){
        level += 1;
        rLen = 1;
        if(rLen >= n) return 0;
    }else{
        goto end;
    }
    
    while(level > 0 && n > rLen){
        (*p)++;
        if(**p == '>'){
            //grab the out part
            (*p)++;
            level -= 1;
        }else if(**p == '<'){
            level += 1;
        }
        rLen++;
    }
    
    end:
    *len = rLen;
    return rLen > 0 ? 1 : 0;
}

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer) */
LEX_PROCESSOR(Lex_String){
    int level = 0;
    int rLen = 0;
    char lastSep = 0;
    int reverseSlash = 0;
    *len = 0;
    *head = *p;
    LEX_DEBUG("Starting String parser at \'%c\'\n", **p);
    
    if(**p == '\'' || **p == '\"'){
        level += 1;
        lastSep = **p;
        rLen = 1;
        if(rLen >= n) return 0;
    }else{
        goto end;
    }
    
    while(level > 0 && n > rLen){
        (*p)++;
        if(**p == '\\'){
            reverseSlash = 1;
        }else if(**p == lastSep && !reverseSlash){
            //grab the out part
            (*p)++;
            level -= 1;
        }else{
            reverseSlash = 0;
        }
        rLen++;
    }
    
    end:
    *len = rLen;
    return rLen > 0 ? 1 : 0;
}

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer) */
LEX_PROCESSOR(Lex_Comments){
    int aggregate = tokenizer->aggregate;
    int type = tokenizer->type;
    //token->identifier = TOKEN_ID_COMMENT;
    
    int rLen = 0;
    char prev = 0;
    *len = 0;
    *head = *p;
    if(aggregate == 0){
        if(n < 2) return 0;
        if(**p == '/' && *((*p)+1) == '/'){
            type = 1;
        }else if(**p == '/' &&  *((*p)+1) == '*'){
            type = 2;
            aggregate = 1;
        }else{
            goto end;
        }
        
        (*p) += 2;
        rLen += 2;
    }
    
    while(rLen < n){
        if(type == 2){
            if(**p == '/' && prev == '*'){
                (*p) ++;
                rLen ++;
                aggregate = 0;
                goto end;
            }else{
#if 0
                if(TerminatorChar(**p)){
                    goto consume;
                }
#endif
                prev = **p;
                (*p) ++;
                rLen++;
            }
        }else{
            if(rLen+1 <= n){
                char s = *(*(p)+1);
                if(**p == '\\' && (s == 0 || s == '\n')){
                    aggregate = 1;
                    (*p)++;
                    rLen++;
                    goto end;
                }
            }
#if 0
            if(TerminatorChar(**p)){
                goto consume;
            }
#endif
            (*p)++;
            rLen++;
            aggregate = 0;
        }
    }
    
    consume:
    if(rLen > 0){
        
    }
    
    end:
    *len = rLen;
    tokenizer->aggregate = aggregate;
    tokenizer->type = type;
    return rLen > 0 ? (aggregate > 0 ? 3 : 1) : 0;
}

/* (char **p, uint n, Token *token, TokenLookupTable *lookup, TokenizerFetchCallback *fetcher, TokenizerContext *context, Tokenizer *tokenizer) */
LEX_PROCESSOR_TABLE(Lex_TokenLookupAny){
    char *h = *p;
    int matched = 0;
    uint length = 1;
    token->identifier = TOKEN_ID_NONE;
    
    if(TerminatorChar(**p)){
        (*p)++;
        goto consume;
    }
    
    // 1- select token
    while(*p != nullptr && length < n){
        (*p)++;
        if(TerminatorChar(**p)){
            goto consume;
        }else{
            length++;
        }
    }
    
    consume:
    // 2- check against lookup table
    token->size = length;
    int tableIndex = -1;
    for(int k = 0; k < lookup->nSize; k++){
        if(length == lookup->sizes[k].y){
            tableIndex = k;
            break;
        }
    }
    
    if(tableIndex >= 0){
        LookupToken *tList = lookup->table[tableIndex];
        int count = lookup->sizes[tableIndex].x;
        for(int k = 0; k < count; k++){
            if(StringEqual(h, tList[k].value, length)){
                token->identifier = tList[k].identifier;
                matched = 1;
                break;
            }
        }
    }
    
    //TODO: Check for other cases
    //      1 - Entering level '{'
    //      2 - Exiting  level '}'
    //      3 - Entering a function '('
    //      4 - Exiting a function ')'
    // do we need to parse these or can we simply loop during execution?
    // going to check for '(' to at least detect functions
    if(!matched){
        if(length > 0){
            if(tokenizer->lastToken.identifier == TOKEN_ID_PREPROCESSOR_DEFINE){
                // this thing is comming after a #define, mark it as another token?
                token->identifier = TOKEN_ID_PREPROCESSOR_DEFINITION;
                if(tokenizer->trackSymbols){
                    Symbol s = {
                        .bufferId = tokenizer->runningLine,
                        .tokenId = (uint)tokenizer->givenTokens,
                        .id = token->identifier,
                    };
                    
                    uint rid = SymbolTable_PushSymbol(&tokenizer->symbolTable, &s, h, 
                                                      length,TOKEN_ID_PREPROCESSOR_DEFINITION);
                    TokenizerCacheSym(rid, tokenizer);
                }
            }else if(!(length == 1 && TerminatorChar(**p))){
                uint maxn = n - length;
                char nextC = Lex_LookAhead(*p, length, maxn, fetcher);
                if(nextC == '('){
                    token->identifier = TOKEN_ID_FUNCTION;
                }
            }
        }
    }else{ // Add logical processors
        if(!Lex_ProcStackInsert(tokenizer, token)){
            // check matched object for symbol table
            if(Lex_IsUserToken(token) && tokenizer->trackSymbols){
                // push this token as a duplicate
                
                Symbol s = {
                    .bufferId = tokenizer->runningLine,
                    .tokenId = (uint)tokenizer->givenTokens,
                    .id = token->identifier,
                };
                
                SymbolTable_PushSymbol(&tokenizer->symbolTable, &s, h, length,
                                       token->identifier);
            }
        }
    }
    
    return length;
}

/* (char **p, uint n, Token *token, uint *offset, TokenizerContext *context, TokenizerWorkContext *workContext, Tokenizer *tokenizer) */
LEX_TOKENIZER_EXEC_CONTEXT(Lex_TokenizeExecCode){
    //1 - Attempt others first
    int rv = TOKENIZER_OP_FINISHED;
    TokenizerFetchCallback *fetcher = tokenizer->fetcher;
    int lexrv = 0;
    char *h = *p;
    uint len = 1;
    uint currSize = 0;
    
    token->size = 1;
    token->identifier = TOKEN_ID_NONE;
    token->source = LEX_CONTEXT_ID_EXEC_CODE;
    
    if(n == 1 && **p == '\\'){
        rv = TOKENIZER_OP_UNFINISHED;
        goto end;
    }
    
    lexrv = Lex_Comments(p, n, &h, &len, context, token, tokenizer);
    if(lexrv == 3){
        token->size = len;
        token->identifier = TOKEN_ID_COMMENT;
        rv = TOKENIZER_OP_UNFINISHED;
        goto end;
    }else if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_COMMENT;
        goto end;
    }
    
    lexrv = Lex_String(p, n, &h, &len, context, token, tokenizer);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_STRING;
        goto end;
    }
    
    lexrv = Lex_Number(p, n, &h, &len, context, token, tokenizer);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_NUMBER;
        goto end;
    }
    
    len = Lex_TokenLookupAny(p, n, token, context->lookup, fetcher, context, tokenizer);
    
    end:
    *offset = len;
    context->has_pending_work = 0;
    if(rv == TOKENIZER_OP_UNFINISHED){
        context->has_pending_work = 1;
    }
    return rv;
}


/* (char **p, uint n, Token *token, uint *offset, TokenizerContext *context, TokenizerWorkContext *workContext, Tokenizer *tokenizer) */
LEX_TOKENIZER_EXEC_CONTEXT(Lex_TokenizeExecCodePreprocessor){
    //1 - Attempt others first
    int rv = TOKENIZER_OP_FINISHED;
    TokenizerFetchCallback *fetcher = tokenizer->fetcher;
    int lexrv = 0;
    char *h = *p;
    uint len = 1;
    uint currSize = 0;
    
    token->size = 1;
    token->identifier = TOKEN_ID_NONE;
    token->source = LEX_CONTEXT_ID_PREPROCESSOR;
    
    if(n == 1 && **p == '\\'){
        rv = TOKENIZER_OP_UNFINISHED;
        goto end;
    }
    
    lexrv = Lex_Comments(p, n, &h, &len, context, token, tokenizer);
    if(lexrv == 3){
        token->size = len;
        token->identifier = TOKEN_ID_COMMENT;
        rv = TOKENIZER_OP_UNFINISHED;
        goto end;
    }else if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_COMMENT;
        goto end;
    }
    
    lexrv = Lex_String(p, n, &h, &len, context, token, tokenizer);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_STRING;
        goto end;
    }
    
    if(tokenizer->inclusion){
        lexrv = Lex_Inclusion(p, n, &h, &len, context, token, tokenizer);
        if(lexrv > 0){
            token->size = len;
            token->identifier = TOKEN_ID_INCLUDE;
            goto end;
        }
    }
    
    lexrv = Lex_Number(p, n, &h, &len, context, token, tokenizer);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_NUMBER;
        goto end;
    }
    
    len = Lex_TokenLookupAny(p, n, token, context->lookup, fetcher, context, tokenizer);
    if(token->identifier == TOKEN_ID_INCLUDE_SEL){
        token->identifier = TOKEN_ID_PREPROCESSOR;
        tokenizer->inclusion = 1;
    }
    
    end:
    *offset = len;
    context->has_pending_work = 0;
    if(rv == TOKENIZER_OP_UNFINISHED){
        context->has_pending_work = 1;
    }
    return rv;
}

/* (char **p, uint n, Token *token, Tokenizer *tokenizer, int process_tab, uint bufferId) */
LEX_TOKENIZER(Lex_TokenizeNext){
    uint offset = 0;
    uint len = 0;
    int rv = 0;
    char *h = *p;
    //printf("Starting at \'%c\'\n", **p);
    
    TokenizerContext *tCtx = nullptr;
    int unfinished = tokenizer->unfinishedContext;
    int lineBeginning = tokenizer->lineBeginning;
    
    tokenizer->lineBeginning = 0;
    tokenizer->unfinishedContext = -1;
    
    // Compute the amount of lines being grouped by unfinished work
    if(lineBeginning){
        if(unfinished >= 0) tokenizer->linesAggregated ++;
        else tokenizer->linesAggregated = 0;
        tokenizer->autoIncrementor = 0;
    }
    
    if(n == 0){
        tokenizer->unfinishedContext = unfinished;
        return -1;
    }
    
    if(**p == '\n'){
        tokenizer->unfinishedContext = unfinished;
        return -1;
    }
    
    tokenizer->givenTokens ++;
    uint tablen = 0;
    // 1- Skip spaces and encode in our format
    while(**p == ' ' || **p == '\t' || **p == '\r'){
        if(**p == '\t') tablen += tokenizer->tabSpacing-1;
        if(process_tab){
            if(**p == '\t'){
                for(int i = 0; i < tokenizer->tabSpacing-1; i++){
                    AssertA(**p == '\t', "Invalid lexing inside tabs");
                    (*p)++;
                }
            }
        }
        
        offset++;
        (*p)++;
    }
    
    // 2- Check which contexts want to process this line
    if(lineBeginning){
        for(int i = 0; i < tokenizer->contextCount; i++){
            tCtx = &tokenizer->contexts[i];
            tCtx->is_execing = 1;
            if(tCtx->entry){
                tCtx->is_execing = tCtx->entry(p, n-offset, tCtx);
            }
        }
    }
    
    if(offset > 0 /*&& lineBeginning*/){
        tokenizer->unfinishedContext = unfinished;
        token->position = tokenizer->linePosition + tokenizer->autoIncrementor;
        token->size = tablen + offset;
        token->identifier = TOKEN_ID_SPACE;
        tokenizer->linePosition += offset;
        tokenizer->autoIncrementor += tablen;
        //tokenizer->lastToken = *token; // do not register space Tokens for preprocessing
        return offset;
    }
    
    // 3- If some context did not finish its job, let it continue
    if(unfinished >= 0){
        tCtx = &tokenizer->contexts[unfinished];
        rv = tCtx->exec(p, n-offset, token, &len, tCtx, tokenizer);
        if(rv == TOKENIZER_OP_UNFINISHED){
            tokenizer->unfinishedContext = unfinished;
            
            token->position = tokenizer->linePosition + 
                tokenizer->autoIncrementor + offset;
            
            tokenizer->linePosition += offset + len;
            tokenizer->lastToken = *token;
            
            return offset + len;
        }else if(rv == TOKENIZER_OP_FINISHED){
            token->position = tokenizer->linePosition + 
                tokenizer->autoIncrementor + offset;
            
            tokenizer->linePosition += offset + len;
            
            if(!BoundedStack_IsEmpty(tokenizer->procStack)){
                LogicalProcessor *proc = BoundedStack_Peek(tokenizer->procStack);
                (void) proc->proc(tokenizer, token, proc, p);
            }
            
            TokenizerConsumeCacheIfPossible(tokenizer);
            TokenizerUpdateState(tokenizer, token);
            tokenizer->lastToken = *token;
            return offset + len;
        }
    }
    
    if(offset + len >= n){
        printf("Untested condition\n");
        return offset + len;
    }
    
    offset += len;
    
    // 4- Give a chance to other contexts to get a token
    for(int i = 0; i < tokenizer->contextCount; i++){
        tCtx = &tokenizer->contexts[i];
        if(tCtx->is_execing){
            rv = tCtx->exec(p, n-offset, token, &len, tCtx, tokenizer);
            if(rv == TOKENIZER_OP_UNFINISHED){
                tokenizer->unfinishedContext = i;
                token->position = tokenizer->linePosition + 
                    tokenizer->autoIncrementor + offset;
                
                tokenizer->linePosition += offset + len;
                TokenizerUpdateState(tokenizer, token);
                tokenizer->lastToken = *token;
                return offset + len;
            }else if(rv == TOKENIZER_OP_FINISHED){
                token->position = tokenizer->linePosition + 
                    tokenizer->autoIncrementor + offset;
                
                tokenizer->linePosition += offset + len;
                if(!BoundedStack_IsEmpty(tokenizer->procStack)){
                    LogicalProcessor *proc = BoundedStack_Peek(tokenizer->procStack);
                    (void) proc->proc(tokenizer, token, proc, p);
                }
                
                TokenizerConsumeCacheIfPossible(tokenizer);
                TokenizerUpdateState(tokenizer, token);
                tokenizer->lastToken = *token;
                return offset + len;
            }
        }
    }
    
    AssertA(0, "No context generated token for input\n");
    return -1;
}

//TODO: Translate the tokens into a more robust expression
static inline int QueryNextNonReservedToken(Buffer *buffer, int i, int *found){
    *found = 0;
    while(i < buffer->tokenCount){
        Token *token = &buffer->tokens[i];
        if(token->identifier != TOKEN_ID_SPACE && 
           token->identifier != TOKEN_ID_ASTERISK)
        {
            *found = 1;
            break;
        }
        i++;
    }
    
    return i;
}

// TODO: This does not work, needs a symbol table
void Lex_TokenizerReviewClassification(Tokenizer *tokenizer, Buffer *buffer){
    return;
    struct SecondPassContext{
        uint start;
    };
    
    SecondPassContext context = {
        .start = 1,
    };
    
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint plevel = ctx->parenLevel;
    uint blevel = ctx->indentLevel;
    
    
    if(buffer->tokenCount > 0){
        Token *token = &buffer->tokens[0];
        if(token->source == LEX_CONTEXT_ID_PREPROCESSOR) return;
    }
    
    for(uint i = 0; i < buffer->tokenCount; i++){
        int found = 0;
        i = QueryNextNonReservedToken(buffer, i, &found);
        if(found){
            Token *token = &buffer->tokens[i];
            if(blevel > 0 || plevel > 0){
                if((token->identifier == TOKEN_ID_NONE) && context.start == 1){
                    int s = QueryNextNonReservedToken(buffer, i+1, &found);
                    if(found){
                        Token *nextToken = &buffer->tokens[s];
                        if(nextToken->identifier == TOKEN_ID_NONE ||
                           nextToken->identifier == TOKEN_ID_PARENTHESE_CLOSE)
                        {
                            //TODO: Perform symbol table query?
                            token->identifier = TOKEN_ID_DATATYPE_USER_DATATYPE;
                            i = s;
                        }
                    }
                    
                    context.start = 0;
                }else if(token->identifier == TOKEN_ID_SEMICOLON ||
                         token->identifier == TOKEN_ID_PARENTHESE_OPEN ||
                         token->identifier == TOKEN_ID_COMMA)
                {
                    context.start = 1;
                }else{
                    context.start = 0;
                }
            }else if(token->identifier == TOKEN_ID_PARENTHESE_OPEN){
                plevel++;
            }else if(token->identifier == TOKEN_ID_PARENTHESE_CLOSE){
                uint k = plevel;
                plevel = k > 0 ? k - 1 : 0;
            }else if(token->identifier == TOKEN_ID_BRACE_OPEN){
                blevel++;
            }else if(token->identifier == TOKEN_ID_BRACE_CLOSE){
                uint k = blevel;
                blevel = k > 0 ? k - 1 : 0;
            }else{
                // TODO: this is either a user defined macro or datatype at beginning
                // of a function, without consulting a symbol table it is not
                // possible to distinguish. Until we decide if we are going
                // to pursue the symbol table or not let this be a datatype.
                if(token->identifier == TOKEN_ID_NONE){
                    token->identifier = TOKEN_ID_DATATYPE_USER_DATATYPE;
                }
            }
        }
    }
}

void Lex_LineProcess(char *text, uint textsize, Lex_LineProcessorCallback *processor,
                     uint refLine, void *prv)
{
    uint processed = 0;
    char *p = text;
    char *lineStart = p;
    uint lineSize = 0;
    uint lineNr = refLine+1;
    while(p != NULL && *p != EOF && processed < textsize){
        if(*p == '\r'){
            p++;
            processed++;
        }
        
        if(*p == '\n'){
            //*p = 0;
            processor(&lineStart, lineSize+1, lineNr, 
                      processed-lineSize, textsize, prv);
            *p = '\n';
            p++;
            lineStart = p;
            lineSize = 0;
            processed++;
            lineNr++;
        }else{
            p++;
            lineSize += 1;
            processed++;
        }
    }
    
    if(lineSize > 0){
        processor(&lineStart, lineSize+1, lineNr, 
                  processed-lineSize, textsize, prv);
    }
}

/* (char **p, uint n, TokenizerContext *context) */
LEX_TOKENIZER_ENTRY_CONTEXT(Lex_PreprocessorEntry){
    if(context->has_pending_work) return 1;
    if(**p == '#') return 1;
    return 0;
}

/* Slow code */
// auxiliary structure for building the token table.
typedef struct{
    const char *value;
    TokenId identifier;
}GToken; // TODO:At least TRY to parse from this and see if it is ok

/* C/C++ tokens that can happen inside a preprocessor */
std::vector<std::vector<GToken>> cppReservedPreprocessor = {
    {{ .value = "#", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "+", .identifier = TOKEN_ID_MATH }, { .value = "-", .identifier = TOKEN_ID_MATH },
        { .value = ">", .identifier = TOKEN_ID_MORE }, { .value = "<", .identifier = TOKEN_ID_LESS },
        { .value = "/", .identifier = TOKEN_ID_MATH }, { .value = "%", .identifier = TOKEN_ID_MATH },
        { .value = "!", .identifier = TOKEN_ID_MATH }, { .value = "=", .identifier = TOKEN_ID_MATH },
        { .value = "*", .identifier = TOKEN_ID_ASTERISK }, { .value = "&", .identifier = TOKEN_ID_MATH },
        { .value = "|", .identifier = TOKEN_ID_MATH }, { .value = "^", .identifier = TOKEN_ID_MATH },
        { .value = "~", .identifier = TOKEN_ID_MATH }, { .value = ",", .identifier = TOKEN_ID_COMMA },
        { .value = ";", .identifier = TOKEN_ID_SEMICOLON }, { .value = ".", .identifier = TOKEN_ID_MATH },
        { .value = "(", .identifier = TOKEN_ID_PARENTHESE_OPEN }, { .value = ")", .identifier = TOKEN_ID_PARENTHESE_CLOSE },
        { .value = "{", .identifier = TOKEN_ID_BRACE_OPEN }, { .value = "}", .identifier = TOKEN_ID_BRACE_CLOSE },
        { .value = "[", .identifier = TOKEN_ID_BRACKET_OPEN }, { .value = "]", .identifier = TOKEN_ID_BRACKET_CLOSE }, 
        { .value = ":", .identifier = TOKEN_ID_MATH }},
    
    {{ .value = "do", .identifier = TOKEN_ID_OPERATOR },{ .value = "if", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "asm", .identifier = TOKEN_ID_OPERATOR },{ .value = "for", .identifier = TOKEN_ID_OPERATOR },
        { .value = "try", .identifier = TOKEN_ID_OPERATOR },{ .value = "int", .identifier = TOKEN_ID_DATATYPE },
        { .value = "new", .identifier = TOKEN_ID_OPERATOR },{ .value = "and", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "this", .identifier = TOKEN_ID_OPERATOR },{ .value = "char", .identifier = TOKEN_ID_DATATYPE },
        { .value = "long", .identifier = TOKEN_ID_DATATYPE },{ .value = "NULL", .identifier = TOKEN_ID_OPERATOR },
        { .value = "case", .identifier = TOKEN_ID_OPERATOR },{ .value = "true", .identifier = TOKEN_ID_OPERATOR },
        { .value = "else", .identifier = TOKEN_ID_PREPROCESSOR },{ .value = "line", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "goto", .identifier = TOKEN_ID_OPERATOR },
        { .value = "bool", .identifier = TOKEN_ID_DATATYPE },{ .value = "void", .identifier = TOKEN_ID_DATATYPE },
        { .value = "enum", .identifier = TOKEN_ID_DATATYPE_ENUM_DEF },{ .value = "auto", .identifier = TOKEN_ID_DATATYPE },},
    {{ .value = "short", .identifier = TOKEN_ID_DATATYPE },{ .value = "const", .identifier = TOKEN_ID_DATATYPE  },
        { .value = "undef", .identifier = TOKEN_ID_PREPROCESSOR },{ .value = "ifdef", .identifier = TOKEN_ID_PREPROCESSOR }, 
        { .value = "error", .identifier = TOKEN_ID_PREPROCESSOR }, { .value = "endif", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "union", .identifier = TOKEN_ID_OPERATOR },{ .value = "using", .identifier = TOKEN_ID_OPERATOR },
        { .value = "class", .identifier = TOKEN_ID_DATATYPE_CLASS_DEF },{ .value = "final", .identifier = TOKEN_ID_OPERATOR },
        { .value = "false", .identifier = TOKEN_ID_OPERATOR },{ .value = "float", .identifier = TOKEN_ID_DATATYPE },
        { .value = "catch", .identifier = TOKEN_ID_OPERATOR },{ .value = "throw", .identifier = TOKEN_ID_OPERATOR },
        { .value = "while", .identifier = TOKEN_ID_OPERATOR },{ .value = "break", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "define", .identifier = TOKEN_ID_PREPROCESSOR_DEFINE },
        { .value = "ifndef", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "pragma", .identifier = TOKEN_ID_PREPROCESSOR},
        { .value = "friend", .identifier = TOKEN_ID_OPERATOR },{ .value = "inline", .identifier = TOKEN_ID_OPERATOR },
        { .value = "static", .identifier = TOKEN_ID_OPERATOR },{ .value = "sizeof", .identifier = TOKEN_ID_OPERATOR },
        { .value = "and_eq", .identifier = TOKEN_ID_OPERATOR },{ .value = "signed", .identifier = TOKEN_ID_DATATYPE },
        { .value = "delete", .identifier = TOKEN_ID_OPERATOR },{ .value = "switch", .identifier = TOKEN_ID_OPERATOR },
        { .value = "typeid", .identifier = TOKEN_ID_OPERATOR },{ .value = "export", .identifier = TOKEN_ID_OPERATOR },
        { .value = "return", .identifier = TOKEN_ID_OPERATOR },{ .value = "extern", .identifier = TOKEN_ID_OPERATOR },
        { .value = "public", .identifier = TOKEN_ID_OPERATOR },{ .value = "double", .identifier = TOKEN_ID_DATATYPE },
        { .value = "struct", .identifier = TOKEN_ID_DATATYPE_STRUCT_DEF },},
    {{ .value = "include", .identifier = TOKEN_ID_INCLUDE_SEL },
        { .value = "defined", .identifier = TOKEN_ID_OPERATOR },
        { .value = "warning", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "mutable", .identifier = TOKEN_ID_OPERATOR },{ .value = "nullptr", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignas", .identifier = TOKEN_ID_OPERATOR },{ .value = "wchar_t", .identifier = TOKEN_ID_DATATYPE },
        { .value = "virtual", .identifier = TOKEN_ID_OPERATOR },{ .value = "default", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignof", .identifier = TOKEN_ID_OPERATOR },{ .value = "private", .identifier = TOKEN_ID_OPERATOR },
        { .value = "typedef", .identifier = TOKEN_ID_DATATYPE_TYPEDEF_DEF },},
    {{ .value = "decltype", .identifier = TOKEN_ID_OPERATOR },{ .value = "operator", .identifier = TOKEN_ID_OPERATOR },
        { .value = "template", .identifier = TOKEN_ID_OPERATOR }, { .value = "__FILE__", .identifier = TOKEN_ID_RESERVED },
        { .value = "__LINE__", .identifier = TOKEN_ID_RESERVED}, { .value = "__DATE__", .identifier = TOKEN_ID_RESERVED },
        { .value = "__TIME__", .identifier = TOKEN_ID_RESERVED }, { .value = "explicit", .identifier = TOKEN_ID_OPERATOR },
        { .value = "register", .identifier = TOKEN_ID_OPERATOR },{ .value = "requires", .identifier = TOKEN_ID_OPERATOR },
        { .value = "override", .identifier = TOKEN_ID_OPERATOR },{ .value = "typename", .identifier = TOKEN_ID_OPERATOR },
        { .value = "volatile", .identifier = TOKEN_ID_OPERATOR },{ .value = "unsigned", .identifier = TOKEN_ID_DATATYPE },
        { .value = "continue", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "constexpr", .identifier = TOKEN_ID_OPERATOR },{ .value = "protected", .identifier = TOKEN_ID_OPERATOR },
        { .value = "namespace", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "const_cast", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "static_cast", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "dynamic_cast", .identifier = TOKEN_ID_OPERATOR },{ .value = "thread_local", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "static_assert", .identifier = TOKEN_ID_OPERATOR },{ .value = "__TIMESTAMP__", .identifier = TOKEN_ID_RESERVED }},
    {{ .value = "reinterpret_cast", .identifier = TOKEN_ID_OPERATOR }}
};

/* C/C++ tokens that can happen inside a code and are not preprocessor */
std::vector<std::vector<GToken>> cppReservedTable = {
    {{ .value = "+", .identifier = TOKEN_ID_MATH }, { .value = "-", .identifier = TOKEN_ID_MATH },
        { .value = ">", .identifier = TOKEN_ID_MORE }, { .value = "<", .identifier = TOKEN_ID_LESS },
        { .value = "/", .identifier = TOKEN_ID_MATH }, { .value = "%", .identifier = TOKEN_ID_MATH },
        { .value = "!", .identifier = TOKEN_ID_MATH }, { .value = "=", .identifier = TOKEN_ID_MATH },
        { .value = "*", .identifier = TOKEN_ID_ASTERISK }, { .value = "&", .identifier = TOKEN_ID_MATH },
        { .value = "|", .identifier = TOKEN_ID_MATH }, { .value = "^", .identifier = TOKEN_ID_MATH },
        { .value = "~", .identifier = TOKEN_ID_MATH }, { .value = ",", .identifier = TOKEN_ID_COMMA },
        { .value = ";", .identifier = TOKEN_ID_SEMICOLON }, { .value = ".", .identifier = TOKEN_ID_MATH },
        { .value = "(", .identifier = TOKEN_ID_PARENTHESE_OPEN }, { .value = ")", .identifier = TOKEN_ID_PARENTHESE_CLOSE },
        { .value = "{", .identifier = TOKEN_ID_BRACE_OPEN }, { .value = "}", .identifier = TOKEN_ID_BRACE_CLOSE },
        { .value = "[", .identifier = TOKEN_ID_BRACKET_OPEN }, { .value = "]", .identifier = TOKEN_ID_BRACKET_CLOSE }, 
        { .value = ":", .identifier = TOKEN_ID_MATH }},
    {{ .value = "do", .identifier = TOKEN_ID_OPERATOR },{ .value = "if", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "asm", .identifier = TOKEN_ID_OPERATOR },{ .value = "for", .identifier = TOKEN_ID_OPERATOR },
        { .value = "try", .identifier = TOKEN_ID_OPERATOR },{ .value = "int", .identifier = TOKEN_ID_DATATYPE },
        { .value = "new", .identifier = TOKEN_ID_OPERATOR },{ .value = "and", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "this", .identifier = TOKEN_ID_OPERATOR },{ .value = "char", .identifier = TOKEN_ID_DATATYPE },
        { .value = "long", .identifier = TOKEN_ID_DATATYPE },{ .value = "NULL", .identifier = TOKEN_ID_OPERATOR },
        { .value = "case", .identifier = TOKEN_ID_OPERATOR },{ .value = "true", .identifier = TOKEN_ID_OPERATOR },
        { .value = "else", .identifier = TOKEN_ID_OPERATOR },{ .value = "goto", .identifier = TOKEN_ID_OPERATOR },
        { .value = "bool", .identifier = TOKEN_ID_DATATYPE },{ .value = "void", .identifier = TOKEN_ID_DATATYPE },
        { .value = "enum", .identifier = TOKEN_ID_DATATYPE_ENUM_DEF },{ .value = "auto", .identifier = TOKEN_ID_DATATYPE },},
    {{ .value = "short", .identifier = TOKEN_ID_DATATYPE },{ .value = "const", .identifier = TOKEN_ID_DATATYPE  },
        { .value = "union", .identifier = TOKEN_ID_OPERATOR },{ .value = "using", .identifier = TOKEN_ID_OPERATOR },
        { .value = "class", .identifier = TOKEN_ID_DATATYPE_CLASS_DEF },{ .value = "final", .identifier = TOKEN_ID_OPERATOR },
        { .value = "false", .identifier = TOKEN_ID_OPERATOR },{ .value = "float", .identifier = TOKEN_ID_DATATYPE },
        { .value = "catch", .identifier = TOKEN_ID_OPERATOR },{ .value = "throw", .identifier = TOKEN_ID_OPERATOR },
        { .value = "while", .identifier = TOKEN_ID_OPERATOR },{ .value = "break", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "friend", .identifier = TOKEN_ID_OPERATOR },{ .value = "inline", .identifier = TOKEN_ID_OPERATOR },
        { .value = "static", .identifier = TOKEN_ID_OPERATOR },{ .value = "sizeof", .identifier = TOKEN_ID_OPERATOR },
        { .value = "and_eq", .identifier = TOKEN_ID_OPERATOR },{ .value = "signed", .identifier = TOKEN_ID_DATATYPE },
        { .value = "delete", .identifier = TOKEN_ID_OPERATOR },{ .value = "switch", .identifier = TOKEN_ID_OPERATOR },
        { .value = "typeid", .identifier = TOKEN_ID_OPERATOR },{ .value = "export", .identifier = TOKEN_ID_OPERATOR },
        { .value = "return", .identifier = TOKEN_ID_OPERATOR },{ .value = "extern", .identifier = TOKEN_ID_OPERATOR },
        { .value = "public", .identifier = TOKEN_ID_OPERATOR },{ .value = "double", .identifier = TOKEN_ID_DATATYPE },
        { .value = "struct", .identifier = TOKEN_ID_DATATYPE_STRUCT_DEF },},
    {{ .value = "mutable", .identifier = TOKEN_ID_OPERATOR },{ .value = "nullptr", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignas", .identifier = TOKEN_ID_OPERATOR },{ .value = "wchar_t", .identifier = TOKEN_ID_DATATYPE },
        { .value = "virtual", .identifier = TOKEN_ID_OPERATOR },{ .value = "default", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignof", .identifier = TOKEN_ID_OPERATOR },{ .value = "private", .identifier = TOKEN_ID_OPERATOR },
        { .value = "typedef", .identifier = TOKEN_ID_DATATYPE_TYPEDEF_DEF },},
    {{ .value = "decltype", .identifier = TOKEN_ID_OPERATOR },{ .value = "operator", .identifier = TOKEN_ID_OPERATOR },
        { .value = "template", .identifier = TOKEN_ID_OPERATOR }, { .value = "__FILE__", .identifier = TOKEN_ID_RESERVED },
        { .value = "__LINE__", .identifier = TOKEN_ID_RESERVED}, { .value = "__DATE__", .identifier = TOKEN_ID_RESERVED },
        { .value = "__TIME__", .identifier = TOKEN_ID_RESERVED }, { .value = "explicit", .identifier = TOKEN_ID_OPERATOR },
        { .value = "register", .identifier = TOKEN_ID_OPERATOR },{ .value = "requires", .identifier = TOKEN_ID_OPERATOR },
        { .value = "override", .identifier = TOKEN_ID_OPERATOR },{ .value = "typename", .identifier = TOKEN_ID_OPERATOR },
        { .value = "volatile", .identifier = TOKEN_ID_OPERATOR },{ .value = "unsigned", .identifier = TOKEN_ID_DATATYPE },
        { .value = "continue", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "constexpr", .identifier = TOKEN_ID_OPERATOR },{ .value = "protected", .identifier = TOKEN_ID_OPERATOR },
        { .value = "namespace", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "const_cast", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "static_cast", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "dynamic_cast", .identifier = TOKEN_ID_OPERATOR },{ .value = "thread_local", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "static_assert", .identifier = TOKEN_ID_OPERATOR },{ .value = "__TIMESTAMP__", .identifier = TOKEN_ID_RESERVED }},
    {{ .value = "reinterpret_cast", .identifier = TOKEN_ID_OPERATOR }},
};

void Lex_BuildTokenLookupTable(TokenLookupTable *lookupTable,
                               std::vector<std::vector<GToken>> *cppTable)
{
    AssertA(lookupTable != nullptr, "Invalid lookup table given");
    AssertA(lookupTable->table == nullptr, 
            "Re-generation of lookup table is not supported");
    
    std::vector<GToken> tokens = cppTable->at(0);
    int elements = cppTable->size();
    int offset = tokens.size();
    
    lookupTable->realSize = elements + DefaultAllocatorSize;
    lookupTable->table = (LookupToken **)AllocatorGet(sizeof(LookupToken *) * lookupTable->realSize);
    lookupTable->sizes = (vec3i *)AllocatorGet(sizeof(vec3i) * lookupTable->realSize);
    
    for(int i = 0; i < elements; i++){
        LookupToken *tokenList = nullptr;
        tokens = cppTable->at(i);
        tokenList = (LookupToken *)AllocatorGet(sizeof(LookupToken) * (tokens.size() + DefaultAllocatorSize));
        for(int k = 0; k < tokens.size(); k++){
            GToken gToken = tokens[k];
            uint len = Min(strlen(gToken.value), TOKEN_MAX_LENGTH);
            Memcpy((void *)tokenList[k].value, (void *)gToken.value, len);
            tokenList[k].identifier = gToken.identifier;
            tokenList[k].size = len;
        }
        
        lookupTable->table[i] = tokenList;
        lookupTable->sizes[i] = vec3i(tokens.size(), tokenList[0].size,
                                      tokens.size() + DefaultAllocatorSize);
    }
    
    lookupTable->nSize = elements;
}

void Lex_PushTokenIntoTable(TokenLookupTable *lookup, char *value,
                            uint size, TokenId id)
{
    LookupToken *tokenList = nullptr;
    if(size == 0) return;
    
    size = Min(TOKEN_MAX_LENGTH, size);
    vec3i l;
    int tableIndex = -1;
    for(uint i = 0; i < lookup->nSize; i++){
        if(size == lookup->sizes[i].y){
            tableIndex = i;
            break;
        }
    }
    
    if(tableIndex < 0){
        // Check if we need to expand the table
        if(!(lookup->nSize + 1 < lookup->realSize)){ // need to expand
            uint oldSize = lookup->realSize;
            lookup->realSize += DefaultAllocatorSize;
            lookup->table = AllocatorExpand(LookupToken *, lookup->table, lookup->realSize);
            lookup->sizes = AllocatorExpand(vec3i, lookup->sizes, lookup->realSize);
        }
        
        tokenList = lookup->table[lookup->nSize];
        if(tokenList == NULL){
            tokenList = AllocatorGetN(LookupToken, DefaultAllocatorSize);
        }
        tableIndex = lookup->nSize;
        l = vec3i(0, size, DefaultAllocatorSize);
        lookup->nSize++;
    }else{
        tokenList = lookup->table[tableIndex];
        l = lookup->sizes[tableIndex];
        if(!(l.x + 1 < l.z)){
            l.z += DefaultAllocatorSize;
            tokenList = AllocatorExpand(LookupToken, tokenList, l.z);
        }
    }
    
    Memcpy(tokenList[l.x].value, value, size);
    tokenList[l.x].identifier = id;
    tokenList[l.x].size = size;
    l.x += 1;
    
    lookup->sizes[tableIndex] = l;
    lookup->table[tableIndex] = tokenList;
}

void Lex_BuildTokenizer(Tokenizer *tokenizer, int tabSpacing, int trackSymbols){
    TokenLookupTable *tables = nullptr;
    TokenizerWorkContext *workContext = nullptr;
    TokenLookupTable *ax = nullptr;
    AssertA(tokenizer != nullptr, "Invalid tokenizer context given");
    tokenizer->contexts = AllocatorGetN(TokenizerContext, 2);
    tokenizer->contextCount = 2;
    tokenizer->unfinishedContext = 0;
    tokenizer->linesAggregated = 0;
    tokenizer->linePosition = 0;
    tokenizer->lineBeginning = 0;
    tokenizer->tabSpacing = Max(1, tabSpacing);
    tokenizer->aggregate = 0;
    tokenizer->type = 0;
    tokenizer->inclusion = 0;
    tokenizer->runningIndentLevel = 0;
    tokenizer->runningParenIndentLevel = 0;
    
    tables = (TokenLookupTable *)AllocatorGet(sizeof(TokenLookupTable) * 
                                              tokenizer->contextCount);
    
    workContext = (TokenizerWorkContext *)AllocatorGet(sizeof(TokenizerWorkContext));
    workContext->workTokenList = AllocatorGetN(Token, 32);
    workContext->workTokenListSize = 32;
    workContext->workTokenListHead = 0;
    workContext->lastToken = nullptr;
    
    tokenizer->workContext = workContext;
    for(int i = 0; i < tokenizer->contextCount; i++){
        tokenizer->contexts[i].lookup = &tables[i];
        tokenizer->contexts[i].is_execing = 0;
        tokenizer->contexts[i].has_pending_work = 0;
    }
    
    Lex_BuildTokenLookupTable(tokenizer->contexts[0].lookup, &cppReservedPreprocessor);
    Lex_BuildTokenLookupTable(tokenizer->contexts[1].lookup, &cppReservedTable);
    
    tokenizer->contexts[0].entry = Lex_PreprocessorEntry;
    tokenizer->contexts[0].exec = Lex_TokenizeExecCodePreprocessor;
    tokenizer->contexts[1].entry = nullptr;
    tokenizer->contexts[1].exec = Lex_TokenizeExecCode;
    tokenizer->unfinishedContext = -1;
    tokenizer->runningLine = 0;
    
    tokenizer->trackSymbols = trackSymbols;
    tokenizer->procStack = BoundedStack_Create();
    tokenizer->commitedSymsCount = 0;
    if(tokenizer->trackSymbols){
        SymbolTable_Initialize(&tokenizer->symbolTable);
    }
}

void Lex_TokenizerGetCurrentState(Tokenizer *tokenizer, TokenizerStateContext *context){
    AssertA(tokenizer != nullptr && context != nullptr,
            "Invalid inputs for Lex_TokenizerGetCurrentState");
    context->state = (tokenizer->unfinishedContext < 0) ? TOKENIZER_STATE_CLEAN : TOKENIZER_STATE_MULTILINE;
    context->activeWorkProcessor = tokenizer->unfinishedContext;
    context->backTrack = 0;
    context->inclusion = tokenizer->inclusion;
    context->aggregate = tokenizer->aggregate;
    context->type = tokenizer->type;
    context->indentLevel = tokenizer->runningIndentLevel;
    context->parenLevel = tokenizer->runningParenIndentLevel;
    BoundedStack_Copy(&context->procStack, tokenizer->procStack);
    if(tokenizer->unfinishedContext >= 0){
        context->backTrack = tokenizer->linesAggregated+1;
    }
}

void Lex_TokenizerRestoreFromContext(Tokenizer *tokenizer,TokenizerStateContext *context){
    AssertA(tokenizer != nullptr && context != nullptr,
            "Invalid inputs for Lex_TokenizerGetCurrentState");
    tokenizer->unfinishedContext = context->activeWorkProcessor;
    tokenizer->linesAggregated = context->backTrack > 0 ? context->backTrack-1 : 0;
    
    tokenizer->inclusion = context->inclusion;
    tokenizer->aggregate = context->aggregate;
    tokenizer->type = context->type;
    tokenizer->runningIndentLevel = context->indentLevel;
    tokenizer->runningParenIndentLevel = context->parenLevel;
    BoundedStack_Copy(tokenizer->procStack, &context->procStack);
    //printf("Stack size: %d\n", BoundedStack_Size(tokenizer->procStack));
}

int Lex_TokenizerHasPendingWork(Tokenizer *tokenizer){
    return (tokenizer->unfinishedContext >= 0 ? 1 : 0 || !BoundedStack_IsEmpty(tokenizer->procStack));
}

void Lex_TokenizerSetFetchCallback(Tokenizer *tokenizer,
                                   TokenizerFetchCallback *callback)
{
    tokenizer->fetcher = callback;
}

void Lex_TokenizerPrepareForNewLine(Tokenizer *tokenizer, uint lineNo){
    tokenizer->linePosition = 0;
    tokenizer->lineBeginning = 1;
    tokenizer->runningLine = lineNo;
    tokenizer->givenTokens = -1;
    tokenizer->inclusion = 0;
    for(int i = 0; i < tokenizer->contextCount; i++){
        tokenizer->contexts[i].is_execing = 0;
    }
}