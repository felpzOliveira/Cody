#include <utilities.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

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
    
    ret = (char *)malloc(filesize);
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
