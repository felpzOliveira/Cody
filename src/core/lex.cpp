#include <stdio.h>
#include <string.h>
#include <lex.h>
#include <sstream>
#include <utilities.h>
#include <vector>

#define MODULE_NAME "Lex"

#define INC_OR_ZERO(p, n, r) do{ if((r) < n) (*p)++; else return 0; }while(0)

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context) */
LEX_PROCESSOR(Lex_Number){
    (void)context;
    int iNum = 0;
    int dotCount = 0;
    int eCount = 0;
    int rLen = 0;
    int iValue = 0;
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
        }else if(**p == 'f'){
            if(dotCount < 1){
                LEX_DEBUG("\'f\' found without \'.\'\n");
                goto end;
            }
            
            rLen++;
            (*p)++;
            goto end;
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
        
        iNum = iValue >= 0 && iValue <= 9 ? 1 : 0;
        if(iNum){
            rLen ++;
            (*p)++;
        }
    }while(iNum != 0 && rLen < n);
    
    end:
    *len = rLen;
    return rLen > 0 ? 1 : 0;
}

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context) */
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

LEX_PROCESSOR(Lex_String){ /* (char **p, size_t n, char **head, size_t *len) */
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

/* (char **p, size_t n, char **head, size_t *len, TokenizerContext *context) */
LEX_PROCESSOR(Lex_Comments){
    int aggregate = context->aggregate;
    int type = context->type;
    
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
            
            (*p)++;
            rLen++;
            aggregate = 0;
        }
    }
    
    end:
    *len = rLen;
    context->aggregate = aggregate;
    context->type = type;
    return rLen > 0 ? (aggregate > 0 ? 3 : 1) : 0;
}

/* (char **p, uint n, Token *token, TokenLookupTable *lookup) */
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
        if(**p == '(' && length > 1){
            token->identifier = TOKEN_ID_FUNCTION;
        }
    }
    
    return length;
}

/* (char **p, uint n, Token *token, uint *offset, TokenizerContext *context, TokenizerWorkContext *workContext) */
LEX_TOKENIZER_EXEC_CONTEXT(Lex_TokenizeExecCode){
    //1 - Attempt others first
    int rv = TOKENIZER_OP_FINISHED;
    int lexrv = 0;
    char *h = *p;
    uint len = 1;
    uint currSize = 0;
    
    token->size = 1;
    token->identifier = TOKEN_ID_NONE;
    
    lexrv = Lex_Comments(p, n, &h, &len, context);
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
    
    lexrv = Lex_String(p, n, &h, &len, context);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_STRING;
        goto end;
    }
    
    lexrv = Lex_Number(p, n, &h, &len, context);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_NUMBER;
        goto end;
    }
    
    len = Lex_TokenLookupAny(p, n, token, context->lookup);
    
    end:
    *offset = len;
    return rv;
}


/* (char **p, uint n, Token *token, uint *offset, TokenizerContext *context, TokenizerWorkContext *workContext) */
LEX_TOKENIZER_EXEC_CONTEXT(Lex_TokenizeExecCodePreprocessor){
    //1 - Attempt others first
    int rv = TOKENIZER_OP_FINISHED;
    int lexrv = 0;
    char *h = *p;
    uint len = 1;
    uint currSize = 0;
    
    token->size = 1;
    token->identifier = TOKEN_ID_NONE;
    
    lexrv = Lex_Comments(p, n, &h, &len, context);
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
    
    lexrv = Lex_String(p, n, &h, &len, context);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_STRING;
        goto end;
    }
    
    if(context->inclusion){
        lexrv = Lex_Inclusion(p, n, &h, &len, context);
        if(lexrv > 0){
            token->size = len;
            token->identifier = TOKEN_ID_INCLUDE;
            goto end;
        }
    }
    
    lexrv = Lex_Number(p, n, &h, &len, context);
    if(lexrv > 0){
        token->size = len;
        token->identifier = TOKEN_ID_NUMBER;
        goto end;
    }
    
    len = Lex_TokenLookupAny(p, n, token, context->lookup);
    if(token->identifier == TOKEN_ID_INCLUDE_SEL){
        token->identifier = TOKEN_ID_PREPROCESSOR;
        context->inclusion = 1;
    }
    
    end:
    *offset = len;
    context->has_pending_work = 0;
    if(rv == TOKENIZER_OP_UNFINISHED){
        context->has_pending_work = 1;
    }
    return rv;
}

/* (char **p, uint n, Token *token, Tokenizer *tokenizer) */
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
    }
    
    if(n == 0){
        tokenizer->unfinishedContext = unfinished;
        return -1;
    }
    
    // 1- Skip spaces
    while(**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r'){
        (*p)++;
        offset ++;
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
        token->position = tokenizer->linePosition;
        token->size = offset;
        token->identifier = TOKEN_ID_SPACE;
        tokenizer->linePosition += offset;
        return offset;
    }
    
#if 0
    /* This line is ending in a space, lets not generate this token */
    if(offset >= n){
        AssertA(0, "Makes no sense");
        tokenizer->unfinishedContext = unfinished;
        return -1;
    }
#endif
    
    // 3- If some context did not finish its job, let it continue
    if(unfinished >= 0){
        tCtx = &tokenizer->contexts[unfinished];
        rv = tCtx->exec(p, n-offset, token, &len, tCtx);
        if(rv == TOKENIZER_OP_UNFINISHED){
            tokenizer->unfinishedContext = unfinished;
            token->position = tokenizer->linePosition + offset;
            tokenizer->linePosition += offset + len;
            return offset + len;
        }else if(rv == TOKENIZER_OP_FINISHED){
            token->position = tokenizer->linePosition + offset;
            tokenizer->linePosition += offset + len;
            return offset + len;
        }
    }
    
    if(offset + len >= n) return offset + len;
    
    offset += len;
    
    // 4- Give a chance to other contexts to get a token
    for(int i = 0; i < tokenizer->contextCount; i++){
        tCtx = &tokenizer->contexts[i];
        if(tCtx->is_execing){
            rv = tCtx->exec(p, n-offset, token, &len, tCtx);
            if(rv == TOKENIZER_OP_UNFINISHED){
                tokenizer->unfinishedContext = i;
                token->position = tokenizer->linePosition + offset;
                tokenizer->linePosition += offset + len;
                return offset + len;
            }else if(rv == TOKENIZER_OP_FINISHED){
                token->position = tokenizer->linePosition + offset;
                tokenizer->linePosition += offset + len;
                return offset + len;
            }
        }
    }
    
    AssertA(0, "No context generated token for input\n");
    return -1;
}

void Lex_LineProcess(char *text, uint textsize, Lex_LineProcessorCallback *processor,
                     void *prv)
{
    uint processed = 0;
    char *p = text;
    char *lineStart = p;
    uint lineSize = 0;
    uint lineNr = 1;
    while(p != NULL && *p != EOF && processed < textsize){
        if(*p == '\r'){
            p++;
            processed++;
        }
        
        if(*p == '\n'){
            *p = 0;
            processor(&lineStart, lineSize, lineNr, prv);
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
        processor(&lineStart, lineSize, lineNr, prv);
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
    {{ .value = "#", .identifier = TOKEN_ID_PREPROCESSOR }},
    {{ .value = "if", .identifier = TOKEN_ID_PREPROCESSOR },},
    {{ .value = "line", .identifier = TOKEN_ID_PREPROCESSOR },
        {.value = "else", .identifier = TOKEN_ID_PREPROCESSOR },},
    {{ .value = "undef", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "ifdef", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "error", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "endif", .identifier = TOKEN_ID_PREPROCESSOR },},
    {{ .value = "define", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "ifndef", .identifier = TOKEN_ID_PREPROCESSOR },
        { .value = "pragma", .identifier = TOKEN_ID_PREPROCESSOR}},
    {{ .value = "include", .identifier = TOKEN_ID_INCLUDE_SEL },
        { .value = "defined", .identifier = TOKEN_ID_OPERATOR },
        { .value = "warning", .identifier = TOKEN_ID_PREPROCESSOR },}
};

/* C/C++ tokens that can happen inside a code and are not preprocessor */
std::vector<std::vector<GToken>> cppReservedTable = {
    {{ .value = "+", .identifier = TOKEN_ID_MATH }, { .value = "-", .identifier = TOKEN_ID_MATH },
        { .value = ">", .identifier = TOKEN_ID_MATH }, { .value = "<", .identifier = TOKEN_ID_MATH },
        { .value = "/", .identifier = TOKEN_ID_MATH }, { .value = "%", .identifier = TOKEN_ID_MATH },
        { .value = "!", .identifier = TOKEN_ID_MATH }, { .value = "=", .identifier = TOKEN_ID_OPERATOR },
        { .value = "*", .identifier = TOKEN_ID_MATH }, { .value = "&", .identifier = TOKEN_ID_MATH },
        { .value = "|", .identifier = TOKEN_ID_MATH }, { .value = "^", .identifier = TOKEN_ID_MATH },
        { .value = "~", .identifier = TOKEN_ID_MATH },},
    {{ .value = "do", .identifier = TOKEN_ID_OPERATOR },{ .value = "if", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "asm", .identifier = TOKEN_ID_OPERATOR },{ .value = "for", .identifier = TOKEN_ID_OPERATOR },
        { .value = "try", .identifier = TOKEN_ID_OPERATOR },{ .value = "int", .identifier = TOKEN_ID_DATATYPE },
        { .value = "new", .identifier = TOKEN_ID_OPERATOR },{ .value = "and", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "this", .identifier = TOKEN_ID_OPERATOR },{ .value = "char", .identifier = TOKEN_ID_DATATYPE },
        { .value = "long", .identifier = TOKEN_ID_DATATYPE },{ .value = "NULL", .identifier = TOKEN_ID_OPERATOR },
        { .value = "case", .identifier = TOKEN_ID_OPERATOR },{ .value = "true", .identifier = TOKEN_ID_OPERATOR },
        { .value = "else", .identifier = TOKEN_ID_OPERATOR },{ .value = "goto", .identifier = TOKEN_ID_OPERATOR },
        { .value = "bool", .identifier = TOKEN_ID_DATATYPE },{ .value = "void", .identifier = TOKEN_ID_DATATYPE },
        { .value = "enum", .identifier = TOKEN_ID_OPERATOR },{ .value = "auto", .identifier = TOKEN_ID_DATATYPE },},
    {{ .value = "short", .identifier = TOKEN_ID_DATATYPE },{ .value = "const", .identifier = TOKEN_ID_DATATYPE  },
        { .value = "union", .identifier = TOKEN_ID_OPERATOR },{ .value = "using", .identifier = TOKEN_ID_OPERATOR },
        { .value = "class", .identifier = TOKEN_ID_OPERATOR },{ .value = "final", .identifier = TOKEN_ID_OPERATOR },
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
        { .value = "struct", .identifier = TOKEN_ID_OPERATOR },},
    {{ .value = "mutable", .identifier = TOKEN_ID_OPERATOR },{ .value = "nullptr", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignas", .identifier = TOKEN_ID_OPERATOR },{ .value = "wchar_t", .identifier = TOKEN_ID_DATATYPE },
        { .value = "virtual", .identifier = TOKEN_ID_OPERATOR },{ .value = "default", .identifier = TOKEN_ID_OPERATOR },
        { .value = "alignof", .identifier = TOKEN_ID_OPERATOR },{ .value = "private", .identifier = TOKEN_ID_OPERATOR },
        { .value = "typedef", .identifier = TOKEN_ID_OPERATOR },},
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
    
    lookupTable->table = (LookupToken **)AllocatorGet(sizeof(LookupToken *) * elements);
    lookupTable->sizes = (vec2i *)AllocatorGet(sizeof(vec2i) * elements);
    
    for(int i = 0; i < elements; i++){
        LookupToken *tokenList = nullptr;
        tokens = cppTable->at(i);
        tokenList = (LookupToken *)AllocatorGet(sizeof(LookupToken) * tokens.size());
        for(int k = 0; k < tokens.size(); k++){
            GToken gToken = tokens[k];
            tokenList[k].value = strdup(gToken.value);
            tokenList[k].identifier = gToken.identifier;
            tokenList[k].size = strlen(gToken.value);
        }
        
        lookupTable->table[i] = tokenList;
        lookupTable->sizes[i] = vec2i(tokens.size(), tokenList[0].size);
    }
    
    lookupTable->nSize = elements;
}

void Lex_BuildTokenizer(Tokenizer *tokenizer){
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
        tokenizer->contexts[i].aggregate = 0;
        tokenizer->contexts[i].type = 0;
        tokenizer->contexts[i].inclusion = 0;
    }
    
    Lex_BuildTokenLookupTable(tokenizer->contexts[0].lookup, &cppReservedPreprocessor);
    Lex_BuildTokenLookupTable(tokenizer->contexts[1].lookup, &cppReservedTable);
    
    tokenizer->contexts[0].entry = Lex_PreprocessorEntry;
    tokenizer->contexts[0].exec = Lex_TokenizeExecCodePreprocessor;
    tokenizer->contexts[1].entry = nullptr;
    tokenizer->contexts[1].exec = Lex_TokenizeExecCode;
    
    tokenizer->unfinishedContext = -1;
}

void Lex_TokenizerGetCurrentState(Tokenizer *tokenizer, TokenizerStateContext *context){
    AssertA(tokenizer != nullptr && context != nullptr,
            "Invalid inputs for Lex_TokenizerGetCurrentState");
    context->state = (tokenizer->unfinishedContext < 0) ? TOKENIZER_STATE_CLEAN : TOKENIZER_STATE_MULTILINE;
    context->activeWorkProcessor = tokenizer->unfinishedContext;
    context->backTrack = 0;
    if(tokenizer->unfinishedContext >= 0){
        context->backTrack = tokenizer->linesAggregated+1;
    }
}

void Lex_TokenizerRestoreFromContext(Tokenizer *tokenizer,TokenizerStateContext *context){
    AssertA(tokenizer != nullptr && context != nullptr,
            "Invalid inputs for Lex_TokenizerGetCurrentState");
    tokenizer->unfinishedContext = context->activeWorkProcessor;
    tokenizer->linesAggregated = context->backTrack > 0 ? context->backTrack-1 : 0;
}

void Lex_TokenizerReset(Tokenizer *tokenizer){
    tokenizer->unfinishedContext = -1;
    tokenizer->linesAggregated = 0;
    Lex_TokenizerPrepareForNewLine(tokenizer);
}

int Lex_TokenizerHasPendingWork(Tokenizer *tokenizer){
    return tokenizer->unfinishedContext >= 0 ? 1 : 0;
}

void Lex_TokenizerPrepareForNewLine(Tokenizer *tokenizer){
    tokenizer->linePosition = 0;
    tokenizer->lineBeginning = 1;
    for(int i = 0; i < tokenizer->contextCount; i++){
        tokenizer->contexts[i].is_execing = 0;
        tokenizer->contexts[i].inclusion = 0;
    }
}