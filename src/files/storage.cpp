#include <storage.h>
#include <utilities.h>
#include <rpc.h>

#define LOG_MODULE "STORAGE"
#include <log.h>

StorageDevice *storageDevice = nullptr;

StorageDevice *FetchStorageDevice(){
    return storageDevice;
}

void StorageDeviceEarlyInit(){
    // TODO: Add whatever we need here
}

void SetStorageDevice(StorageDeviceType type, const char *ip, int port,
                      SecurityServices::Context *ctx)
{
    if(storageDevice){
        printf("Warning: Asked for storage device swap, this operation is not supported\n");
        return;
    }

    switch(type){
        case StorageDeviceType::Local:{
            storageDevice = new LocalStorageDevice;
        } break;
        case StorageDeviceType::Remote:{
            storageDevice = new RemoteStorageDevice(ip, port, ctx);
        } break;
        default:{
            printf("Unknown storage device type\n");
        }
    }
}

template<typename FileDesc>
static bool FileAppendTo(const char *path, const char *str, int with_line_brk=1){
    FileDesc file;
    bool rv = FileDesc::FromPath(&file, path, FileOpenMode::Append);
    if(!rv) return false;

    rv = file.WriteString(str, with_line_brk);
    rv |= file.CloseFile();
    return rv;
}

//////////////////////////////////////////////////////////////////
//                   L O C A L   S T O R A G E                  //
//i.e.: Read/Write operations on local disk, trivial stuff.     //
//////////////////////////////////////////////////////////////////
char *LocalStorageDevice::GetContentsOf(const char *path, uint *size){
    return GetFileContents(path, size);
}

void LocalStorageDevice::GetWorkingDirectory(char *dir, uint len){
    GetCurrentWorkingDirectory(dir, len);
}

int LocalStorageDevice::ListFiles(char *basePath, FileEntry **entries,
                                  uint *n, uint *size)
{
    return ListFileEntries(basePath, entries, n, size);
}

bool LocalStorageDevice::StreamWriteStart(FileHandle *handle, const char *path){
    bool rv = LocalFile::FromPath(&handle->localFile, path, FileOpenMode::Write);
    if(!rv) return rv;
    handle->type = LocalFile::GetType();
    return true;
}

bool LocalStorageDevice::StreamWriteString(FileHandle *handle, const char *str, int line_brk){
    return handle->localFile.WriteString(str, line_brk);
}

size_t LocalStorageDevice::StreamWriteBytes(FileHandle *handle, void *ptr,
                                            size_t size, size_t nmemb)
{
    return handle->localFile.WriteBytes(ptr, size, nmemb);
}

bool LocalStorageDevice::StreamFinish(FileHandle *handle){
    handle->type = -1;
    return handle->localFile.CloseFile();
}

bool LocalStorageDevice::AppendTo(const char *path, const char *str, int with_line_brk){
    return FileAppendTo<LocalFile>(path, str, with_line_brk);
}

void LocalStorageDevice::CloseFile(FileHandle *handle){
    handle->localFile.CloseFile();
}

//////////////////////////////////////////////////////////////////
//                 R E M O T E   S T O R A G E                  //
//i.e.: Read/Write operations on remote disk, not so trivial.   //
//////////////////////////////////////////////////////////////////
RemoteStorageDevice::RemoteStorageDevice(const char *ip, int port,
                                         SecurityServices::Context *ctx)
{
    client.SetSecurityContext(ctx);
    if(!client.ConnectTo(ip, port)){
        exit(0);
    }
}

int RemoteStorageDevice::Chdir(const char *path){
    std::vector<uint8_t> out;
    return client.Chdir(out, path) ? 0 : -1;
}

int RemoteStorageDevice::ListFiles(char *basePath, FileEntry **entries,
                                   uint *n, uint *size)
{
    std::vector<uint8_t> out;
    FileEntry *lEntries = nullptr;
    uint currSize = 0;
    uint base = 8;
    uint count = 0;
    if(!client.ListFiles(out, basePath)) return -1;

    if(*n == 0 || *entries == nullptr){
        lEntries = AllocatorGetN(FileEntry, base);
        currSize = base;
    }else{
        lEntries = *entries;
        currSize = *n;
    }

    uint s = 0;
    uint8_t *ptr = out.data();
    while(s < out.size()){
        uint32_t length = 0;
        uint8_t id = out[s];

        if(id != DescriptorDirectory && id != DescriptorFile){
            LOG_ERR("Unknown id value " << id);
            return false;
        }

        if(out.size() < s + sizeof(uint32_t)){
            LOG_ERR("Corrupted data - could not find length");
            return false;
        }

        memcpy(&length, &ptr[s+1], sizeof(uint32_t));
        if(out.size() < s + 1 + length){
            LOG_ERR("Corrupted data - could not find content");
            return false;
        }

        if(!(currSize > count + 1)){
            lEntries = AllocatorExpand(FileEntry, lEntries,
                                       currSize+base, currSize);
            currSize += base;
        }

        lEntries[count].type = (FileType)id;
        Memcpy(lEntries[count].path, &ptr[s+1+sizeof(uint32_t)], length);
        lEntries[count].path[length] = 0;
        lEntries[count].pLen = length;
        lEntries[count].isLoaded = 0;
        count++;
        s += 1 + sizeof(uint32_t) + length;
    }

    *entries = lEntries;
    *n = count;
    *size = currSize;

    return 1;
}

void RemoteStorageDevice::GetWorkingDirectory(char *dir, uint len){
    std::vector<uint8_t> out;
    dir[0] = 0;
    if(client.GetPwd(out)){
        if(out.size() < 1 || out.size() > len){
            LOG_ERR("Invalid pwd query?");
            return;
        }
        memcpy(dir, out.data(), out.size());
    }else{
        LOG_ERR("Could not query pwd");
    }
}

char *RemoteStorageDevice::GetContentsOf(const char *path, uint *size){
    std::vector<uint8_t> out;
    if(!client.ReadEntireFile(out, path)){
        printf("Failed to read file\n");
        return nullptr;
    }

    if(out.size() < 1){
        return nullptr;
    }

    char *content = AllocatorGetN(char, out.size());
    memcpy(content, out.data(), out.size());
    *size = out.size()-1;
    return content;
}

bool RemoteStorageDevice::StreamWriteStart(FileHandle *handle, const char *path){
    std::vector<uint8_t> out;
    if(!client.StreamWriteStart(out, path)){
        LOG_ERR("Failed to open file " << path);
        return false;
    }

    handle->type = RemoteFile::GetType();
    return true;
}

size_t RemoteStorageDevice::StreamWriteBytes(FileHandle *handle, void *ptr,
                                             size_t size, size_t nmemb)
{
    std::vector<uint8_t> out;
    uint32_t val = 0;
    if(!client.StreamWriteUpdate(out, (uint8_t *)ptr, size * nmemb, 1)){
        LOG_ERR("Failed to write data to file");
        return 0;
    }

    if(out.size() < sizeof(uint32_t)){
        LOG_ERR("Missing write byte count");
        return 0;
    }

    memcpy(&val, out.data(), sizeof(uint32_t));
    return val;
}

bool RemoteStorageDevice::StreamWriteString(FileHandle *handle, const char *str, int line_brk){
    std::vector<uint8_t> out;
    if(!line_brk){
        uint32_t len = strlen(str);
        if(!client.StreamWriteUpdate(out, (uint8_t *)str, len+1, 0)){
            LOG_ERR("Failed to write string to file");
            return false;
        }
    }else{
        std::string tmp(str);
        tmp += "\n";
        uint32_t len = tmp.size();
        if(!client.StreamWriteUpdate(out, (uint8_t *)tmp.c_str(), len+1, 0)){
            LOG_ERR("Failed to write string to file");
            return false;
        }
    }
    return true;
}

bool RemoteStorageDevice::StreamFinish(FileHandle *handle){
    std::vector<uint8_t> out;
    if(!client.StreamWriteFinal(out)){
        LOG_ERR("Failed to close file");
        return false;
    }
    handle->type = -1;
    return true;
}

bool RemoteStorageDevice::AppendTo(const char *path, const char *str, int with_line_brk){
    std::vector<uint8_t> out;
    uint32_t size = strlen(str) + 1;
    if(!client.AppendTo(out, path, (uint8_t *)str, size, with_line_brk)){
        LOG_ERR("Failed to append");
        return false;
    }
    return true;
}

void RemoteStorageDevice::CloseFile(FileHandle *handle){
    //handle->remoteFile.CloseFile();
}

