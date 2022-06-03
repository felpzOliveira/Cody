/* date = June 1st 2022 22:10 */
#pragma once
#include <stdlib.h>
#include <stdio.h>

#define UNIMPLEMENTED() printf("Unimplemented {%s}\n", __FUNCTION__)

typedef enum{
    Local, Remote
}StorageDeviceType;

typedef enum{
    Write, Append, Read, Closed,
}FileOpenMode;

class LocalFile{
    public:
    bool is_open;
    FILE *fp;
    FileOpenMode open_mode;

    LocalFile(): is_open(false), fp(nullptr), open_mode(FileOpenMode::Closed){}
    ~LocalFile(){ CloseFile(); }

    size_t WriteBytes(void *ptr, size_t size, size_t nmemb){
        if(fp && is_open && ptr){
            size_t s = fwrite(ptr, size, nmemb, fp);
            if(s != nmemb){
                printf("[FileWrite] Error = %s\n", strerror(errno));
            }
            return s;
        }
        return 0;
    }

    size_t ReadBytes(void *ptr, size_t size, size_t nmemb){
        if(fp && ptr && is_open){
            size_t s = fread(ptr, size, nmemb, fp);
            if(s != nmemb){
                printf("[FileRead] Error = %s\n", strerror(errno));
            }
            return s;
        }
        return 0;
    }

    bool WriteString(const char *str, int with_line_brk=1){
        if(str && fp && is_open){
            if(with_line_brk){
                fprintf(fp, "%s\n", str);
            }else{
                fprintf(fp, "%s", str);
            }
            return true;
        }
        return false;
    }

    bool CloseFile(){
        if(fp && is_open){
            fclose(fp);
            fp = nullptr;
            is_open = false;
            open_mode = FileOpenMode::Closed;
        }
        return true;
    }

    bool OpenFile(const char *path, FileOpenMode mode){
        if(mode == FileOpenMode::Write){
            fp = fopen(path, "wb");
        }else if(mode == FileOpenMode::Append){
            fp = fopen(path, "a+");
        }else if(mode == FileOpenMode::Read){
            fp = fopen(path, "rb");
        }else{
            printf("Tried to open file with unknown flag\n");
        }

        open_mode = mode;
        is_open = fp != nullptr;
        return is_open;
    }

    static int GetType(){ return 0; }

    static bool FromPath(LocalFile *file, const char *path, FileOpenMode mode){
        return file->OpenFile(path, mode);
    }
};

class RemoteFile{
    public:
    RemoteFile() = default;
    ~RemoteFile() = default;

    static int GetType(){ return 1; }
};

struct FileHandle{
    int type = -1;
    LocalFile localFile;
    RemoteFile remoteFile;
};

