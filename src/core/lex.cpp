#include <stdio.h>
#include <string.h>
#include <lex.h>
#include <sstream>
#include <utilities.h>
#include <vector>
#include <buffers.h>
#include <languages.h>

#define MODULE_NAME "Lex"

#define INC_OR_ZERO(p, n, r) do{ if((r) < n) (*p)++; else return 0; }while(0)

//TODO: What happens when user insert a new } or ) or whatever and we simply retokenize
//      that line? Do all other parts work?
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
            id == TOKEN_ID_INCLUDE || id == TOKEN_ID_OPERATOR ||
            id == TOKEN_ID_DATATYPE_TYPEDEF_DEF || id == TOKEN_ID_DATATYPE_STRUCT_DEF ||
            id == TOKEN_ID_DATATYPE_CLASS_DEF || id == TOKEN_ID_DATATYPE_ENUM_DEF);
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

int Lex_IsUserToken(Token *token){
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
    if(token->identifier == refId){
        proc->range.x = tokenizer->runningLine;
        return 1;
    }
    
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
                proc->range.y = tokenizer->runningLine;
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
                proc->range.y = tokenizer->runningLine;
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 1;
                filter = 2;
            }
        } break;
        /* possible token */
        case TOKEN_ID_NONE:{
            if(proc->nestedLevel == 0 && proc->currentState == 0){
                token->identifier = TOKEN_ID_DATATYPE_USER_CLASS;
                proc->range.y = tokenizer->runningLine;
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
        
        case TOKEN_ID_BRACE_OPEN:{
            if(proc->nestedLevel == 1 && proc->currentState == 1){
                /*
* We got our token but there is a definition following, allow
* outter components to process.
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
    
    if(grabbed){
        if(SymbolTable_Insert(tokenizer->symbolTable, h,
                              token->size, token->identifier) == 0)
        {
            token->identifier = TOKEN_ID_NONE;
        }else{
            token->reserved = StringDup(h, token->size);
        }
    }
    
    return filter > 1 ? 0 : 1;
}

/* (Tokenizer *tokenizer, Token *token, LogicalProcessor *proc, char **p) */
LEX_LOGICAL_PROCESSOR(Lex_EnumProcessor){
    /*
* enum A{ <Anything> };
* The way we did this thing we will never be able to capture
* 'enum class <Something>{ <Anything> };' because class will consume
* <Something> 
*/
    int emitWarningToken = 0;
    int filter = Lex_TokenLogicalFilter(tokenizer, token, proc,
                                        TOKEN_ID_DATATYPE_ENUM_DEF, p);
    
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
            }else if(proc->nestedLevel == 1){
                /*
* We got one of the values inside the enum, this is a very soft test tho
*/
                char *h = (*p) - token->size;
                token->identifier = TOKEN_ID_DATATYPE_USER_ENUM_VALUE;
                if(SymbolTable_Insert(tokenizer->symbolTable, h,
                                      token->size, token->identifier) == 0)
                {
                    token->identifier = TOKEN_ID_NONE;
                }else{
                    token->reserved = StringDup(h, token->size);
                }
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
                proc->range.y = tokenizer->runningLine;
                BoundedStack_Pop(tokenizer->procStack);
                emitWarningToken = 1;
                filter = 2;
            }
        } break;
        /* possible token */
        case TOKEN_ID_NONE:{
            if(proc->nestedLevel == 0 && proc->currentState == 0){
                proc->range.y = tokenizer->runningLine;
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
    
    if(grabbed){
        if(SymbolTable_Insert(tokenizer->symbolTable, h,
                              token->size, token->identifier) == 0)
        {
            token->identifier = TOKEN_ID_NONE;
        }else{
            token->reserved = StringDup(h, token->size);
        }
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
            proc->range.y = tokenizer->runningLine;
            grabbed = 1;
        }else if(token->identifier == TOKEN_ID_NONE && 
                 proc->nestedLevel == 0 && proc->currentState == 1)
        {
            //token->identifier = TOKEN_ID_DATATYPE_USER_TYPEDEF;
            token->identifier = TOKEN_ID_DATATYPE_USER_DATATYPE;
            proc->range.y = tokenizer->runningLine;
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
    
    if(grabbed){
        if(SymbolTable_Insert(tokenizer->symbolTable, h,
                              token->size, token->identifier) == 0)
        {
            token->identifier = TOKEN_ID_NONE;
        }else{
            token->reserved = StringDup(h, token->size);
        }
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
    size_t rLen = 0;
    int iValue = 0;
    int uCount = 0;
    int lCount = 0;
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
        if(uCount == 1 && (**p == 'L' || **p == 'l') && lCount < 2){
            lCount++;
            rLen ++;
            (*p)++;
            goto _number_start;
        }else if(uCount == 1 && (**p == 'L' || **p == 'l') && lCount >= 2){
            LEX_DEBUG("\'L\' found many times\n");
            goto end;
        }else if(uCount == 1 && !(**p == 'L' || **p == 'l')){
            LEX_DEBUG("\'U\' marks termination \n");
            goto end;
        }else if(**p == 'U' || **p == 'u'){
            if(uCount > 0){
                LEX_DEBUG("\'U\' found many times\n");
                goto end;
            }
            uCount++;
            rLen ++;
            (*p)++;
            goto _number_start;
        }else if(**p == '.'){
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
        }else if((**p == 'e' || **p == 'E') && xCount == 0){
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
    size_t rLen = 0;
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

//TODO: This does not capture the pattern: R"<*>(...)<*>"
/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context, Token *token, Tokenizer *tokenizer) */
LEX_PROCESSOR(Lex_String){
    int level = 0;
    size_t rLen = 0;
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
    
    size_t rLen = 0;
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
    while(*p != nullptr && length <= n){
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
        if((int)length == lookup->sizes[k].y){
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
    
    if(!matched){
        if(length > 0){
            if(tokenizer->lastToken.identifier == TOKEN_ID_PREPROCESSOR_DEFINE){
                // this thing is comming after a #define, mark it as another token?
                token->identifier = TOKEN_ID_PREPROCESSOR_DEFINITION;
                token->reserved = StringDup(h, length);
                SymbolTable_Insert(tokenizer->symbolTable, h, length,
                                   TOKEN_ID_PREPROCESSOR_DEFINITION);
                
            }else if(!(length == 1 && TerminatorChar(**p))){
                uint maxn = n - length;
                char nextC = Lex_LookAhead(*p, length, maxn, fetcher);
                if(nextC == '('){
                    if(tokenizer->runningIndentLevel > 0){
                        token->identifier = TOKEN_ID_FUNCTION;
                    }else{
                        token->identifier = TOKEN_ID_FUNCTION_DECLARATION;
#if 0
                        SymbolTable_Insert(tokenizer->symbolTable, h, length,
                                           TOKEN_ID_FUNCTION_DECLARATION);
#endif
                    }
                }
            }
        }
    }else{ // Add logical processors
        if(!Lex_ProcStackInsert(tokenizer, token)){
            // check matched object for symbol table
            if(Lex_IsUserToken(token)){
                printf("Untested condition\n");
                // push this token as a duplicate
                if(SymbolTable_Insert(tokenizer->symbolTable, h,
                                      length, token->identifier) == 0)
                {
                    token->identifier = TOKEN_ID_NONE;
                }
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
    }else if(token->identifier == TOKEN_ID_PREPROCESSOR_IF){
        if(tokenizer->lastToken.identifier == TOKEN_ID_PREPROCESSOR && 
           tokenizer->lastToken.size == 1)
        {
            token->identifier = TOKEN_ID_PREPROCESSOR;
        }else{
            token->identifier = TOKEN_ID_OPERATOR;
        }
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
    //printf("Starting at \'%c\'\n", **p);
    
    TokenizerContext *tCtx = nullptr;
    int unfinished = tokenizer->unfinishedContext;
    int lineBeginning = tokenizer->lineBeginning;
    
    tokenizer->lineBeginning = 0;
    tokenizer->unfinishedContext = -1;
    token->reserved = nullptr;
    
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
    while(i < (int)buffer->tokenCount){
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

void Lex_BuildTokenLookupTable(TokenLookupTable *lookupTable,
                               std::vector<std::vector<GToken>> *cppTable)
{
    AssertA(lookupTable != nullptr, "Invalid lookup table given");
    AssertA(lookupTable->table == nullptr, 
            "Re-generation of lookup table is not supported");
    
    std::vector<GToken> tokens = cppTable->at(0);
    int elements = cppTable->size();
    
    lookupTable->realSize = elements + DefaultAllocatorSize;
    lookupTable->table = (LookupToken **)AllocatorGet(sizeof(LookupToken *) * lookupTable->realSize);
    lookupTable->sizes = (vec3i *)AllocatorGet(sizeof(vec3i) * lookupTable->realSize);
    
    for(int i = 0; i < elements; i++){
        LookupToken *tokenList = nullptr;
        tokens = cppTable->at(i);
        tokenList = (LookupToken *)AllocatorGet(sizeof(LookupToken) * (tokens.size() + DefaultAllocatorSize));
        for(uint k = 0; k < tokens.size(); k++){
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

void Lex_BuildTokenizer(Tokenizer *tokenizer, int tabSpacing, SymbolTable *symTable){
    TokenLookupTable *tables = nullptr;
    TokenizerWorkContext *workContext = nullptr;
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
    tokenizer->symbolTable = symTable;
    tokenizer->procStack = BoundedStack_Create();
}

void Lex_TokenizerContextReset(Tokenizer *tokenizer){
    tokenizer->unfinishedContext = 0;
    tokenizer->linesAggregated = 0;
    tokenizer->linePosition = 0;
    tokenizer->lineBeginning = 0;
    tokenizer->aggregate = 0;
    tokenizer->type = 0;
    tokenizer->inclusion = 0;
    tokenizer->runningIndentLevel = 0;
    tokenizer->runningParenIndentLevel = 0;
    tokenizer->runningLine = 0;
    tokenizer->unfinishedContext = -1;
    BoundedStack_SetDefault(tokenizer->procStack);
}

void Lex_TokenizerContextEmpty(TokenizerStateContext *context){
    context->state = TOKENIZER_STATE_CLEAN;
    context->activeWorkProcessor = -1;
    context->backTrack = 0;
    context->inclusion = 0;
    context->aggregate = 0;
    context->indentLevel = 0;
    context->parenLevel = 0;
    context->type = 0;
    BoundedStack_SetDefault(&context->procStack);
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
