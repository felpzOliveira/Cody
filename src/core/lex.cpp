#include <stdio.h>
#include <string.h>
#include <lex.h>
#include <sstream>
#include <utilities.h>

#define MODULE_NAME "Lex"

#define INC_OR_ZERO(p, n, r) do{ if((r) < n) (*p)++; else return 0; }while(0)

static int TerminatorChar(char v){
    const char *term = " ,;(){}+-/|><!~:%&*=\n\r\t";
    int size = strlen(term);
    if(v == '\0') return 1;
    for(int i = 0; i < size; i++){
        if(term[i] == v) return 1;
    }
    
    return 0;
}

LEX_PROCESSOR(Lex_Number){ /* (char **p, size_t n, char **head, size_t *len) */
    int iNum = 0;
    int dotCount = 0;
    int eCount = 0;
    int rLen = 0;
    int iValue = 0;
    *len = 0;
    *head = *p;
    
    LEX_DEBUG("Starting at \'%c\'\n", **p);
    
    iValue = (**p) - '0';
    
    if(**p == '.'){
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
            
            if(rLen+1 < n){
                if(!TerminatorChar(*((*p)+1))){
                    LEX_DEBUG("\'f\' found in middle of value\n");
                    goto end;
                }
            }
            
            rLen++;
            (*p)++;
            goto end;
        }else if(**p == 'e'){
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
    
    if(TerminatorChar(**p) && **p != 0){
        (*p)++;
    }
    
    end:
    *len = rLen;
    return rLen > 0 ? 1 : 0;
}

//TODO
LEX_PROCESSOR(Lex_String){
    return 0;
}

/* Slow code */
void Lex_LineProcess(const char *path, Lex_LineProcessorCallback *processor){
    std::ifstream ifs(path);
    std::string linebuf;
    
    if(!ifs.is_open()){
        DEBUG_MSG("Failed to open file %s\n", path);
        return;
    }
    
    while(ifs.peek() != -1){
        const char *str = nullptr;
        char *begin = nullptr;
        int rc = 0;
        GetNextLineFromStream(ifs, linebuf);
        RemoveUnwantedLineTerminators(linebuf);
        if(linebuf.empty()) continue;
        str = linebuf.c_str();
        begin = (char *)str;
        
        rc = processor(&begin, linebuf.size());
        if(rc < 0) break;
    }
    
    ifs.close();
}
