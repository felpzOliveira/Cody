#include <utilities.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <lex.h>
#include <app.h>

Float ColorClamp(Float val){
    return Clamp(val, 0.0f, 0.9999f);
}

uint GetSimplifiedPathName(char *fullPath, uint len){
    uint c = len > 2 ? len - 2 : 0;
    while(c > 0){
        char h = fullPath[c];
        if(h == '\\' || h == '/'){
            return c+1;
        }
        c--;
    }
    
    return 0;
}

void Memset(void *dst, unsigned char v, uint size){
    unsigned char *udst = (unsigned char *)dst;
    while(size --){
        *(udst++) = v;
    }
}

void Memcpy(void *dst, void *src, uint size){
    unsigned char *udst = (unsigned char *)dst;
    unsigned char *usrc = (unsigned char *)src;
    while(size --){
        *(udst++) = *(usrc++);
    }
}

int ExtensionStringContains(const char *string, const char *extensions){
    const char* start = extensions;
    for(;;){
        const char* where;
        const char* terminator;
        
        where = strstr(start, string);
        if(!where)
            return 0;
        
        terminator = where + strlen(string);
        if(where == start || *(where - 1) == ' '){
            if(*terminator == ' ' || *terminator == '\0')
                break;
        }
        
        start = terminator;
    }
    
    return 1;
}

int StringToCodepoint(char *u, int size, int *off){
    int l = size;
    *off = 0;
    if(l < 1) return -1;
    
    unsigned char u0 = u[0];
    
    if(u0 == '\t'){
        *off = appGlobalConfig.tabSpacing;
        return u0;
    }
    
    if(u0 >= 0 && u0 <= 127){
        *off = 1;
        return u0; // Ascii table
    }
    
    if(l < 2) return -1; // if not Ascii we don't known what is this
    
    unsigned char u1 = u[1];
    if(u0 >= 192 && u0 <= 223){
        *off = 2;
        return (u0 - 192) * 64 + (u1 - 128);
    }
    
    if(u[0] == 0xed && (u[1] & 0xa0) == 0xa0)
        return -1; //code points, 0xd800 to 0xdfff
    if(l < 3) return -1;
    
    unsigned char u2 = u[2];
    if(u0 >= 224 && u0 <= 239){
        *off = 3;
        return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
    }
    
    if(l < 4) return -1;
    
    unsigned char u3 = u[3];
    if(u0 >= 240 && u0 <= 247){
        *off = 4;
        return (u0 - 240) * 262144 + (u1 - 128) * 4096 + (u2 - 128) * 64 + (u3 - 128);
    }
    
    return -1;
}

int CodepointToString(int cp, char *c){
    int n = -1;
    memset(c, 0x00, sizeof(char) * 5);
    if(cp <= 0x7F){ 
        c[0] = cp;
        n = 1;
    }else if(cp <= 0x7FF){
        c[0] = (cp >> 6) + 192;
        c[1] = (cp & 63) + 128;
        n = 2;
    }else if(0xd800 <= cp && cp <= 0xdfff){
        n = 0;
        //invalid block of utf8
    }else if(cp <= 0xFFFF){
        c[0] = (cp >> 12) + 224;
        c[1]= ((cp >> 6) & 63) + 128;
        c[2]=(cp & 63) + 128;
        n = 3;
    }else if(cp <= 0x10FFFF){
        c[0] = (cp >> 18) + 240;
        c[1] = ((cp >> 12) & 63) + 128;
        c[2] = ((cp >> 6) & 63) + 128;
        c[3] = (cp & 63) + 128;
        n = 4;
    }
    return n;
}

uint DigitCount(uint value){
    uint v = 0;
    do{
        v++;
        value /= 10;
    }while(value != 0);
    return v;
}

vec4i ColorFromHex(uint hex){
    uint a = (hex & 0xff000000) >> 24;
    uint r = (hex & 0x00ff0000) >> 16;
    uint g = (hex & 0x0000ff00) >> 8;
    uint b = (hex & 0x000000ff);
    return vec4i(r, g, b, a);
}

vec4f ColorFromHexf(uint hex){
    vec4i c = ColorFromHex(hex);
    return vec4f(c.x * kInv255, c.y * kInv255,
                 c.z * kInv255, c.w * kInv255);
}

vec4i ColorFromRGBA(vec4f color){
    return vec4i((uint)(ColorClamp(color.x) * 256.0f),
                 (uint)(ColorClamp(color.y) * 256.0f),
                 (uint)(ColorClamp(color.z) * 256.0f),
                 (uint)(ColorClamp(color.w) * 256.0f));
}

void OUTPUT(int j, char *y, int m){
    char s = y[j + m];
    y[j + m] = 0;
    printf("Found at : %d ( %s )\n", j, y + j);
    y[j + m] = s;
}

void preMp(char *x, int m, int mpNext[]) {
    int i, j;
    
    i = 0;
    j = mpNext[0] = -1;
    while (i < m) {
        while (j > -1 && x[i] != x[j])
            j = mpNext[j];
        mpNext[++i] = ++j;
    }
}


void MP(char *x, int m, char *y, int n) {
    const int XSIZE = 255;
    int i, j, mpNext[XSIZE];
    
    /* Preprocessing */
    preMp(x, m, mpNext);
    
    /* Searching */
    i = j = 0;
    while (j < n) {
        while (i > -1 && x[i] != y[j])
            i = mpNext[i];
        i++;
        j++;
        if (i >= m) {
            OUTPUT(j - i, y, m);
            i = mpNext[i];
        }
    }
}


void preQsBc(char *x, int m, int qsBc[], const int ASIZE) {
    int i;
    
    for (i = 0; i < ASIZE; ++i)
        qsBc[i] = m + 1;
    for (i = 0; i < m; ++i)
        qsBc[x[i]] = m - i;
}


void QS(char *x, int m, char *y, int n) {
    const int ASIZE = 255;
    int j, qsBc[ASIZE];
    
    /* Preprocessing */
    preQsBc(x, m, qsBc, ASIZE);
    
    /* Searching */
    j = 0;
    while (j <= n - m) {
        if(memcmp(x, y + j, m) == 0){
            OUTPUT(j, y, m);
        }
        j += qsBc[y[j + m]];               /* shift */
    }
}

int FastStringSearch(char *s0, char *s1, uint s0len, uint s1len){
    MP(s0, s0len, s1, s1len);
    return 0;
}

int StringEqual(char *s0, char *s1, uint maxn){
    for(uint i = 0; i < maxn; i++){
        if(s0[i] != s1[i]){
            return 0;
        }
    }
    return 1;
}

char *StringDup(char *s0, uint len){
    char *s = AllocatorGetN(char, len+1);
    Memcpy(s, s0, len);
    return s;
}

int TerminatorChar(char v){
    if((int)v < 0) return 0; // utf-8
    return (v < 48 || (v > 57 && v < 64) || 
            (v > 90 && v < 95) || (v > 122))
        ? 1 : 0;
}

void RemoveUnwantedLineTerminators(std::string &line){
    if(line.size() > 0){ //remove '\n'
        if(line[line.size()-1] == '\n') line.erase(line.size() - 1);
    }
    
    if(line.size() > 0){ //remove '\r'
        if(line[line.size()-1] == '\r') line.erase(line.size() - 1);
    }
}

std::istream &GetNextLineFromStream(std::istream &is, std::string &str){
    str.clear();
    std::istream::sentry se(is, true);
    std::streambuf *sb = is.rdbuf();
    if(se){
        for(;;){
            int c = sb->sbumpc();
            switch(c){
                case '\n': return is;
                case '\r': if(sb->sgetc() == '\n') sb->sbumpc(); return is;
                case EOF: if(str.empty()) is.setstate(std::ios::eofbit); return is;
                default: str += static_cast<char>(c);
            }
        }
    }
    
    return is;
}

int GetRightmostSplitter(const char *path, uint size){
    int addr = -1;
    for(uint i = size-1; i >= 0; i--){
        char *p = (char*)&path[i];
        if(*p == '\\' || *p == '/'){
            addr = i;
            break;
        }
    }
    
    return addr;
}

char *GetFileContents(const char *path, uint *size){
    struct stat st;
    uint filesize = 0, bytes = 0;
    char *ret = nullptr;
    
    int fd = open(path, O_RDONLY);
    if(fd == -1) goto _error;
    
    stat(path, &st);
    filesize = st.st_size;
    
    //TODO: Refactor or make cross-platform implementation
    posix_fadvise(fd, 0, 0, 1);
    
    ret = (char *)AllocatorGet(filesize+1); // make sure it has null terminator
    if(!ret) goto _error_memory;
    
    bytes = read(fd, ret, filesize);
    if(bytes != filesize) goto _error_memory;
    
    close(fd);
    *size = bytes;
    return ret;
    
    _error_memory:
    if(ret){
        free(ret);
        ret = nullptr;
    }
    
    _error:
    if(fd >= 0) close(fd);
    return ret;
}

BoundedStack *BoundedStack_Create(){
    BoundedStack *stack = (BoundedStack *)AllocatorGet(sizeof(BoundedStack));
    AssertA(stack != nullptr, "Failed to get stack memory");
    stack->top = -1;
    stack->capacity = MAX_BOUNDED_STACK_SIZE;
    return stack;
}

void BoundedStack_Copy(BoundedStack *dst, BoundedStack *src){
    dst->capacity = src->capacity;
    dst->top = src->top;
    Memcpy(dst->items, src->items, sizeof(LogicalProcessor) * src->capacity);
}

int BoundedStack_IsFull(BoundedStack *stack){
    return stack->top == stack->capacity - 1;
}

int BoundedStack_Size(BoundedStack *stack){
    return stack->top + 1;
}

int BoundedStack_IsEmpty(BoundedStack *stack){
    return stack->top == -1;
}

void BoundedStack_Push(BoundedStack *stack, LogicalProcessor *item){
    AssertA(!BoundedStack_IsFull(stack), "Stack is full");
    Memcpy(&stack->items[++stack->top], item, sizeof(LogicalProcessor));
}

LogicalProcessor *BoundedStack_Peek(BoundedStack *stack){
    if(BoundedStack_IsEmpty(stack)){
        return nullptr;
    }else{
        return &stack->items[stack->top];
    }
}

void BoundedStack_Pop(BoundedStack *stack){
    if(!BoundedStack_IsEmpty(stack)){
        stack->top--;
    }
}
