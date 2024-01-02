#include <utilities.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <lex.h>
#include <limits.h>
#include <sys/stat.h>
#include <sstream>
#include <encoding.h>
#if !defined(_WIN32)
#include <unistd.h>
#include <dirent.h>

FileType SymlinkGetType(const char *path){
    char tmp[2048];
    ssize_t r = readlink(path, tmp, 2048);
    if(r < 0){
        return DescriptorFile;
    }

    struct stat st;
    if(stat(tmp, &st) == 0){
        if(st.st_mode & S_IFDIR) return DescriptorDirectory;
    }

    return DescriptorFile;
}
#else
#include <Windows.h>

char* realpath(const char* path, char* resolved_path) {
    if (path == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    DWORD length = GetFullPathNameA(path, 0, NULL, NULL);
    if (length == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    char* buffer = (char*)malloc(length * sizeof(char));
    if (buffer == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    DWORD result = GetFullPathNameA(path, length, buffer, NULL);
    if (result == 0 || result >= length) {
        free(buffer);
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (resolved_path != NULL) {
        strcpy(resolved_path, buffer);
        free(buffer);
        return resolved_path;
    }

    return buffer;
}

#define DT_DIR 1
#define DT_REG -1
#define DT_LNK 2

typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA findData;
} DIR;

struct dirent {
    char d_name[MAX_PATH];
    int d_type;
};

DIR* opendir(const char* path) {
    DIR* dir = (DIR*)malloc(sizeof(DIR));

    char searchPath[MAX_PATH];
    snprintf(searchPath, MAX_PATH, "%s\\*", path);

    dir->handle = FindFirstFileA(searchPath, &dir->findData);

    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }

    return dir;
}

struct dirent* readdir(DIR* dir) {
    if (dir == NULL || dir->handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    if (FindNextFileA(dir->handle, &dir->findData) != 0) {
        static struct dirent entry;
        strncpy(entry.d_name, (const char *)dir->findData.cFileName, sizeof(entry.d_name));
        if (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            entry.d_type = 1;
        else
            entry.d_type = 2;
        return &entry;
    }

    return NULL;
}

void closedir(DIR* dir) {
    if (dir != NULL && dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
        free(dir);
    }
}

char *getcwd(char *dir, uint len){
    if(GetCurrentDirectoryA(len, dir) == 0)
        return nullptr;
    return dir;
}

// TODO
FileType SymlinkGetType(const char *path){
    return DescriptorFile;
}

#endif

#define IS_DIGIT(x) (static_cast<unsigned int>((x) - '0') < static_cast<unsigned int>(10))

void SwapPathDelimiter(std::string &path){
    for(uint i = 0; i < path.size(); i++){
        if(path[i] == '/' || path[i] == '\\'){
            path[i] = SEPARATOR_CHAR;
        }
    }
}

int Mkdir(const char *path){
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0777);
#endif
}

char* __realpath(const char* path, char* resolved_path){
    return realpath(path, resolved_path);
}

uint Bad_RNG16(){
    return rand() % 0x0000FFFF;
}

Float ColorClamp(Float val){
    return Clamp(val, 0.0f, 0.9999f);
}

void GetCurrentWorkingDirectory(char *dir, uint len){
    if(getcwd(dir, len) == nullptr){
        printf("Failed to get CWD\n");
    }
}

std::string StringReplace(std::string str, const std::string& from, const std::string& to){
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos){
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

bool StringStartsWithInteger(std::string str){
    bool rv = true;
    for(uint i = 0; i < str.size(); i++){
        char v = str[i] - '0';
        if(str[i] == ' ' || str[i] == '\n' || str[i] == ')'){
            break;
        }

        if(!(v >= 0 && v <= 9)){
            rv = false;
            break;
        }
    }

    return rv;
}

std::string FetchSecondLine(std::string s){
    std::string res;
    int where = -1;
    for(uint i = 0; i < s.size(); i++){
        if(s[i] == '\n'){
            where = i+1;
            break;
        }
    }

    if(where > 0){
        res = StringTrim(s.substr(where));
    }

    return res;
}

std::string StringTrim(std::string s){
    uint ns = 0, ne = s.size();
    for(uint i = 0; i < s.size(); i++){
        if(s[i] != ' ' && s[i] != '\n' && s[i] != '\t'){
            ns = i;
            break;
        }
    }

    for(uint i = s.size()-1; i > ns; i--){
        if(s[i] != ' ' && s[i] != '\n' && s[i] != '\t'){
            ne = i+1;
            break;
        }
    }

    if(ns != ne){
        return s.substr(ns, ne);
    }

    return s;
}

std::string ExpandFilePath(char *path, uint size, char *folder){
    // for now a soft test solves our problems
    // TODO: PathCchCanonicalize
    for(uint i = 0; i < size; i++){
        if(path[i] == '\\' || path[i] == '/'){
            return std::string(path);
        }
    }

    std::string res(folder);
    char last = res[res.size()-1];
    if(last != '/' && last != '\\'){
        res += SEPARATOR_STRING;
    }

    res += path;
    return res;
}

int FileExists(char *path){
    FILE *fp = fopen(path, "rb");
    bool exists = fp != nullptr;
    if(fp) fclose(fp);
    return exists;
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

int ListFileEntriesLinear(char *basePath, std::vector<uint8_t> &out, uint32_t *size){
    AssertA(basePath != nullptr, "Invalid query pointer");
    DIR *dir = opendir(basePath);
    struct dirent *entry = nullptr;
    if(dir == nullptr) return -1;

    uint8_t mem[32];
    if(size) *size = 0;
    do{
        entry = readdir(dir);
        if(entry != nullptr){
            if(entry->d_type == DT_DIR || entry->d_type == DT_REG ||
               entry->d_type == DT_LNK)
            {
                int keep = 1;
                char *p = entry->d_name;
                uint reclen = strlen(p);
                if(reclen == 1){
                    keep = p[0] != '.' ? 1 : 0;
                }else if(reclen == 2){
                    keep = (p[0] == '.' && p[1] == '.') ? 0 : 1;
                }

                if(keep){
                    uint8_t id = DescriptorFile;
                    if(entry->d_type == DT_DIR){
                        id = DescriptorDirectory;
                    }else if(entry->d_type == DT_LNK){
                        id = SymlinkGetType(p);
                    }
                    memcpy(mem, &reclen, sizeof(uint32_t));

                    out.push_back(id);
                    out.insert(out.end(), &mem[0], &mem[sizeof(uint32_t)]);
                    out.insert(out.end(), &p[0], &p[reclen]);
                    if(size) *size += 1;
                }
            }
        }
    }while(entry != nullptr);
    closedir(dir);

    return 1;
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
            if(entry->d_type == DT_DIR || entry->d_type == DT_REG ||
               entry->d_type == DT_LNK)
            {
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
                    }else if(entry->d_type == DT_LNK){
                        lEntries[count].type = SymlinkGetType(p);
                        //printf("File %s\n", entry->d_name);
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

int StringFindFirstChar(char *s0, uint s0len, char v){
    for(int i = 0; i < (int)s0len; i++){
        if(s0[i] == v) return i;
    }
    return -1;
}

int StringFindLastChar(char *s0, uint s0len, char v){
    for(int i = (int)s0len-1; i >= 0; i--){
        if(s0[i] == v) return i;
    }
    return -1;
}

int StringFirstNonEmpty(char *s0, uint s0len){
    for(uint i = 0; i < s0len; i++){
        // TODO: other empty chars?
        if(s0[i] != ' ') return (int)i;
    }
    return -1;
}

char *StringNextWord(char *s0, uint s0len, uint *len){
    bool space_found = false;
    int remaining = (int)s0len;
    while(remaining > 0){
        char v = s0[s0len-remaining];
        if(!space_found){
            space_found = v == ' ';
        }else{
            if(v != ' '){
                break;
            }
        }

        remaining -= 1;
    }

    if(remaining > 0){
        int rest = remaining;
        *len = 0;
        while(rest > 0){
            char v = s0[s0len - remaining + *len];
            if(v == ' ' || v == 0){
                break;
            }

            (*len)++;
            rest--;
        }
        return &s0[s0len-remaining];
    }
    return nullptr;
}

int StringStartsWith(char *s0, uint s0len, char *s1, uint s1len){
    if(s1len > s0len) return 0;
    for(uint i = 0; i < s1len; i++){
        if(s0[i] != s1[i]) return 0;
    }

    return 1;
}

uint StringComputeU8Count(EncoderDecoder *encoder, char *s0, uint len){
    uint r = 0;
    if(len > 0){
        char *p = s0;
        uint c = 0;
        int rv = -1;
        do{
            int of = 0;
            rv = StringToCodepoint(encoder, &p[c], len - c, &of);
            if(rv != -1){
                c += of;
                r += 1;
            }
        }while(*p && c < len && rv >= 0);
    }

    return r;
}

// The following routine is from tinyobjloader.
bool StringToFloat(char *s, char *s_end, Float *result){
    if(s >= s_end){
        return false;
    }

    double mantissa = 0.0;
    // This exponent is base 2 rather than 10.
    // However the exponent we parse is supposed to be one of ten,
    // thus we must take care to convert the exponent/and or the
    // mantissa to a * 2^E, where a is the mantissa and E is the
    // exponent.
    // To get the final double we will use ldexp, it requires the
    // exponent to be in base 2.
    int exponent = 0;

    // NOTE: THESE MUST BE DECLARED HERE SINCE WE ARE NOT ALLOWED
    // TO JUMP OVER DEFINITIONS.
    char sign = '+';
    char exp_sign = '+';
    char const *curr = s;

    // How many characters were read in a loop.
    int read = 0;
    // Tells whether a loop terminated due to reaching s_end.
    bool end_not_reached = false;

    /*
            BEGIN PARSING.
    */

    // Find out what sign we've got.
    if(*curr == '+' || *curr == '-'){
        sign = *curr;
        curr++;
    }else if (IS_DIGIT(*curr)){ /* Pass through. */
    }else{
        goto fail;
    }

    // Read the integer part.
    end_not_reached = (curr != s_end);
    while(end_not_reached && IS_DIGIT(*curr)){
        mantissa *= 10;
        mantissa += static_cast<int>(*curr - 0x30);
        curr++;
        read++;
        end_not_reached = (curr != s_end);
    }

    // We must make sure we actually got something.
    if(read == 0)
        goto fail;
    // We allow numbers of form "#", "###" etc.
    if(!end_not_reached)
        goto assemble;

    // Read the decimal part.
    if(*curr == '.'){
        curr++;
        read = 1;
        end_not_reached = (curr != s_end);
        while(end_not_reached && IS_DIGIT(*curr)){
            static const double pow_lut[] = {
                1.0, 0.1, 0.01, 0.001, 0.0001, 0.00001, 0.000001, 0.0000001,
            };
            const int lut_entries = sizeof pow_lut / sizeof pow_lut[0];

            // NOTE: Don't use powf here, it will absolutely murder precision.
            mantissa += static_cast<int>(*curr - 0x30) *
            (read < lut_entries ? pow_lut[read] : std::pow(10.0, -read));
            read++;
            curr++;
            end_not_reached = (curr != s_end);
        }
    }else if(*curr == 'e' || *curr == 'E'){
    }else{
        goto assemble;
    }

    if(!end_not_reached)
        goto assemble;

    // Read the exponent part.
    if(*curr == 'e' || *curr == 'E'){
        curr++;
        // Figure out if a sign is present and if it is.
        end_not_reached = (curr != s_end);
        if(end_not_reached && (*curr == '+' || *curr == '-')){
            exp_sign = *curr;
            curr++;
        }else if(IS_DIGIT(*curr)){ /* Pass through. */
        }else{
            // Empty E is not allowed.
            goto fail;
        }

        read = 0;
        end_not_reached = (curr != s_end);
        while(end_not_reached && IS_DIGIT(*curr)){
            exponent *= 10;
            exponent += static_cast<int>(*curr - 0x30);
            curr++;
            read++;
            end_not_reached = (curr != s_end);
        }

        exponent *= (exp_sign == '+' ? 1 : -1);
        if(read == 0)
            goto fail;
    }

assemble:
    *result = (sign == '+' ? 1 : -1) *
                (exponent ? std::ldexp(mantissa * std::pow(5.0, exponent), exponent)
                    : mantissa);
    return true;
fail:
    return false;
}

uint StringCompressPath(char *path, uint size, uint width){
    uint at = 0;
    uint counter = 0;
    for(at = size-1; at > 0 && counter < width; at--){
        if(path[at] == '/' || path[at] == '\\') counter++;
    }

    // at + 1 => '/'
    if(counter == width) return at + 2;
    return 0;
}

uint StringComputeRawPosition(EncoderDecoder *encoder, char *s0, uint len, uint u8p, int *size){
    uint r = 0;
    if(len > 0 && s0){
        char *p = s0;
        int c = 0;
        int of = 0;
        if(u8p == 0){
            if(size){
                StringToCodepoint(encoder, &p[c], len, &of);
                *size = of;
            }
            return 0;
        }

        while(r != u8p){
            of = 0;
            int rv = StringToCodepoint(encoder, &p[c], len - c, &of);
            if(rv == -1) break;
            r ++;
            c += of;
        }

        if(r != u8p){
            BUG();
            printf("Could not compute raw position for %s - %d( u8 = %u )\n",
                   s0, (int)len, u8p);
            return 0;
        }

        if(size){
            *size = 1;
            if((int)len > c){
                (void)StringToCodepoint(encoder, &p[c], len - c, &of);
                *size = of;
            }
        }
        r = (uint)c;
    }else{
        if(len) *size = 1;
    }
    return r;
}

uint StringComputeCharU8At(EncoderDecoder *encoder, char *s0, CharU8 *chr, uint at, int len){
    char *p = s0;
    int c = 0;
    int rv = -1;
    int of = 0;
    if(len < 0){
        len = strlen(s0);
    }

    do{
        rv = StringToCodepoint(encoder, &p[c], len - c, &of);
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

void StringSplit(std::string s, std::vector<std::string> &output, char value){
    std::string val;
    bool on_space = false;
    for(uint i = 0; i < s.size(); i++){
        if(s[i] == value){
            if(!on_space){
                on_space = true;
                if(val.size() > 0){
                    output.push_back(val);
                    val.clear();
                }
            }
        }else{
            on_space = false;
            val.push_back(s[i]);
        }
    }

    if(val.size() > 0){
        output.push_back(val);
    }
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

// TODO: Add as needed
int StopChar(char v){
    return (v == ' ' || v == '+' || v == '-' || v == '/' ||  v == '.' ||
            v == '\"' || v == '\'' || v == '*' || v == '&' || v == '!' ||
            v == '|' || v == '(' || v == ')' || v == '{' || v == '}' ||
            v == '[' || v == ']' || v == ';' || v == ',' || v == '<' ||
            v == '>' || v == '\t' || v == ':' || v == '\\' || v == '\n');
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
    for(int i = size-1; i >= 0; i--){
        char *p = (char*)&path[i];
        if(*p == '\\' || *p == '/'){
            addr = i;
            break;
        }
    }

    return addr;
}

char *GetFileContents(const char *path, uint *size){
    char *ret = nullptr;
    FILE *fp = fopen(path, "rb");
    if(fp == nullptr)
        return nullptr;

    fseek(fp, 0, SEEK_END);
    long memsize = ftell(fp);
    rewind(fp);

    ret = (char *)AllocatorGet(memsize+1);
    long readN = fread(ret, 1, memsize, fp);

    if(readN != memsize)
        goto _error_memory;

    *size = memsize;
    return ret;
_error_memory:
    if(ret){
        AllocatorFree(ret);
        ret = nullptr;
    }
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
