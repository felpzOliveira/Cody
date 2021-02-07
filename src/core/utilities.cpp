#include <utilities.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

vec4ui AutoColor(vec3ui color){
    return vec4ui(color.x, color.y, color.z, 255);
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

vec3i ColorFromHex(uint hex){
    uint r = (hex & 0x00ff0000) >> 16;
    uint g = (hex & 0x0000ff00) >> 8;
    uint b = (hex & 0x000000ff);
    return vec3i(r, g, b);
}

vec3f ColorFromHexf(uint hex){
    vec3i c = ColorFromHex(hex);
    return vec3f(c.x / 255.0f, c.y / 255.0f, c.z / 255.0f);
}

int StringEqual(char *s0, char *s1, uint maxn){
    for(uint i = 0; i < maxn; i++){
        if(s0[i] != s1[i]){
            return 0;
        }
    }
    return 1;
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
