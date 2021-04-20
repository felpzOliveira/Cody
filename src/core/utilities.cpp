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
#include <limits.h>
#include <dirent.h>

Float ColorClamp(Float val){
    return Clamp(val, 0.0f, 0.9999f);
}

void GetCurrentWorkingDirectory(char *dir, uint len){
    if(getcwd(dir, len) == nullptr){
        printf("Failed to get CWD\n");
    }
}

//TODO: Windows
int GuessFileEntry(char *path, uint size, FileEntry *entry, char *folder){
    int r = -1;
    if(path && size > 0 && entry && folder){
        struct stat st;
        if(stat(path, &st) == 0){
            if(st.st_mode & S_IFDIR || st.st_mode & S_IFREG){
                char *ptr = realpath(path, folder);
                if(ptr){
                    uint s = strlen(folder);
                    uint n = GetSimplifiedPathName(folder, s);
                    Memcpy(entry->path, &folder[n], s - n);
                    entry->path[s-n] = 0;
                    entry->pLen = s - n;
                    entry->isLoaded = 0;
                    if(st.st_mode & S_IFDIR){
                        entry->type = DescriptorDirectory;
                    }else{
                        if(n > 0)
                            folder[n-1] = 0;
                        entry->type = DescriptorFile;
                    }

                    r = 0;
                }
            }
        }
    }

    return r;
}

int ListFileEntries(char *basePath, FileEntry **entries, uint *n, uint *size){
    AssertA(n != nullptr && entries != nullptr && basePath != nullptr,
            "Invalid query pointers");
    FileEntry *lEntries = nullptr;
    uint base = 8;
    uint count = 0;
    uint currSize = 0;
    DIR *dir = opendir(basePath);
    struct dirent *entry = nullptr;
    
    if(dir == nullptr) return -1;
    
    if(*n == 0 || *entries == nullptr){
        lEntries = AllocatorGetN(FileEntry, base);
        currSize = base;
    }else{
        lEntries = *entries;
        currSize = *n;
    }
    
    do{
        entry = readdir(dir);
        if(entry != nullptr){
            if(entry->d_type == DT_DIR || entry->d_type == DT_REG){
                int keep = 1;
                char *p = entry->d_name;
                uint reclen = strlen(p);
                if(reclen == 1){
                    keep = p[0] != '.' ? 1 : 0;
                }else if(reclen == 2){
                    keep = (p[0] == '.' && p[1] == '.') ? 0 : 1;
                }
                
                if(keep){
                    if(!(currSize > count + 1)){
                        lEntries = AllocatorExpand(FileEntry, lEntries,
                                                   currSize+base, currSize);
                        currSize += base;
                    }
                    
                    if(entry->d_type == DT_DIR){
                        lEntries[count].type = DescriptorDirectory;
                        //printf("Directory %s (len = %d)\n", entry->d_name, (int) reclen);
                    }else{
                        lEntries[count].type = DescriptorFile;
                        //printf("File %s\n", entry->d_name);
                    }
                    
                    Memcpy(lEntries[count].path, p, reclen);
                    lEntries[count].path[reclen] = 0;
                    lEntries[count].pLen = reclen;
                    lEntries[count].isLoaded = 0;
                    count++;
                }
            }
        }
    }while(entry != nullptr);
    closedir(dir);
    
    *entries = lEntries;
    *n = count;
    *size = currSize;
    return 1;
}

uint GetSimplifiedPathName(char *fullPath, uint len){
    uint c = len > 0 ? len - 1 : 0;
    while(c > 0){
        char h = fullPath[c];
        if(h == '\\' || h == '/'){
            return c+1;
        }
        c--;
    }
    
    return 0;
}

uint StringComputeU8Count(char *s0, uint len){
    uint r = 0;
    if(len > 0){
        char *p = s0;
        uint c = 0;
        int rv = -1;
        do{
            int of = 0;
            rv = StringToCodepoint(&p[c], len - c, &of);
            if(rv != -1){
                c += of;
                r += 1;
            }
        }while(*p && c < len && rv >= 0);
    }

    return r;
}

uint StringComputeCharU8At(char *s0, CharU8 *chr, uint at, int len){
    char *p = s0;
    int c = 0;
    int rv = -1;
    int of = 0;
    if(len < 0){
        len = strlen(s0);
    }

    do{
        rv = StringToCodepoint(&p[c], len - c, &of);
        if(rv != -1){
            if(c <= (int)at && c + of >= (int)at){
                break;
            }

            c += of;
        }
    }while(*p && rv >= 0 && len > c);

    Memcpy(chr->x, &p[c], of);
    chr->x[of] = 0;
    return (uint)of;
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

uint GetDigitOf(uint value, uint n){
    uint v = 0;
    n += 1;
    while(n-- > 0 && value != 0){
        v = value % 10;
        value /= 10;
    }

    return v;
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

vec4f ColorFromInt(vec4i col){
    return vec4f(col.x * kInv255, col.y * kInv255,
                 col.z * kInv255, col.w * kInv255);
}

vec4i ColorFromRGBA(vec4f color){
    return vec4i((uint)(ColorClamp(color.x) * 256.0f),
                 (uint)(ColorClamp(color.y) * 256.0f),
                 (uint)(ColorClamp(color.z) * 256.0f),
                 (uint)(ColorClamp(color.w) * 256.0f));
}

const int XSIZE = 255;
int mpNext[XSIZE];
void FastStringSearchInit(char *x, uint m){
    int i, j;
    i = 0;
    j = mpNext[0] = -1;
    while(i < (int)m){
        while(j > -1 && x[i] != x[j])
            j = mpNext[j];
        mpNext[++i] = ++j;
    }
}

int MP(char *x, int m, char *y, int n){
    int i, j;
    i = j = 0;
    while(j < n){
        while(i > -1 && x[i] != y[j])
            i = mpNext[i];
        i++;
        j++;
        if(i >= m){
            return j-i;
        }
    }
    
    return -1;
}

int QS(char *x, int m, char *y, int n) {
    int j = 0;
    while (j <= n - m) {
        if(StringEqual(x, y + j, m)){
            return j;
        }
        j++;
    }
    
    return -1;
}

int RQS(char *x, int m, char *y, int n){
    int j = n - m;
    while(j >= 0){
        if(StringEqual(x, y + j, m)){
            return j;
        }
        j--;
    }
    return -1;
}

int ReverseStringSearch(char *s0, char *s1, uint s0len, uint s1len){
    return RQS(s0, s0len, s1, s1len);
}

int StringSearch(char *s0, char *s1, uint s0len, uint s1len){
    return QS(s0, s0len, s1, s1len);
}

int FastStringSearch(char *s0, char *s1, uint s0len, uint s1len){
    return MP(s0, s0len, s1, s1len);
}

int StringIsDigits(char *s0, uint len){
    for(uint i = 0; i < len; i++){
        char v = s0[i] - '0';
        if(!(v >= 0 && v <= 9)) return 0;
    }
    
    return 1;
}

uint StringToUnsigned(char *s0, uint len){
    uint n = 0;
    unsigned long maxn = UINT_MAX-1;
    long int test = 0;
    char *ptr = nullptr;
    test = strtoul(s0, &ptr, 10);
    if(ptr == s0){
        printf("invalid number conversion (%s)\n", s0);
    }else if((test == LONG_MIN || test == LONG_MAX) && errno == ERANGE){
        printf("invalid number conversion (range) (%s)\n", s0);
    }else if((unsigned long) test > maxn){
        n = maxn;
    }else{
        n = (uint)test;
    }
    
    return n;
}

int GetFilePathExtension(const char *path, uint size){
    if(size < 2) return -1;
    for(int i = (int)size-1; i >= 0; i--){
        if(path[i] == '.') return i;
    }

    return -1;
}

int StringEqual(char *s0, char *s1, uint maxn){
    for(uint i = 0; i < maxn; i++){
        if(s0[i] != s1[i]){
            return 0;
        }
    }
    return 1;
}

String *StringMake(char *s0, uint len){
    String *str = AllocatorGetN(String, 1);
    str->data = StringDup(s0, len);
    str->size = len;
    return str;
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
        AllocatorFree(ret);
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

void BoundedStack_SetDefault(BoundedStack *stack){
    AssertA(stack != nullptr, "Invalid stack pointer");
    stack->top = -1;
    stack->capacity = MAX_BOUNDED_STACK_SIZE;
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
