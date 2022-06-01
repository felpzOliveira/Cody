#include <storage.h>
#include <utilities.h>
#include <rpc.h>

StorageDevice *storageDevice = nullptr;
StorageDevice *backupDevice = nullptr;

StorageDevice *FetchStorageDevice(){
    return storageDevice;
}

StorageDevice *FetchBackupStorageDevice(){
    return backupDevice;
}

void StorageDeviceEarlyInit(){
    backupDevice = new LocalStorageDevice;
}

void SetStorageDevice(StorageDeviceType type, const char *ip, int port){
    if(storageDevice){
        printf("Warning: Asked for storage device swap, this operation is not supported\n");
        return;
    }

    switch(type){
        case StorageDeviceType::Local:{
            storageDevice = new LocalStorageDevice;
        } break;
        case StorageDeviceType::Remote:{
            storageDevice = new RemoteStorageDevice(ip, port);
            // TODO: Setup ip/port
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

bool LocalStorageDevice::StreamReadStart(FileHandle *handle, const char *path){
    bool rv = LocalFile::FromPath(&handle->localFile, path, FileOpenMode::Read);
    if(!rv) return rv;
    handle->type = LocalFile::GetType();
    return true;
}

bool LocalStorageDevice::StreamWriteStart(FileHandle *handle, const char *path){
    bool rv = LocalFile::FromPath(&handle->localFile, path, FileOpenMode::Write);
    if(!rv) return rv;
    handle->type = LocalFile::GetType();
    return true;
}

bool LocalStorageDevice::StreamWriteString(FileHandle *handle, const char *str){
    return handle->localFile.WriteString(str, 0);
}

size_t LocalStorageDevice::StreamWriteBytes(FileHandle *handle, void *ptr,
                                            size_t size, size_t nmemb)
{
    return handle->localFile.WriteBytes(ptr, size, nmemb);
}

size_t LocalStorageDevice::StreamReadBytes(FileHandle *handle, void *ptr,
                                           size_t size, size_t nmemb)
{
    return handle->localFile.ReadBytes(ptr, size, nmemb);
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
RemoteStorageDevice::RemoteStorageDevice(const char *ip, int port){
    client.ConnectTo(ip, port);
}

int RemoteStorageDevice::ListFiles(char *basePath, FileEntry **entries,
                                   uint *n, uint *size)
{
    UNIMPLEMENTED();
    return 0;
}

void RemoteStorageDevice::GetWorkingDirectory(char *dir, uint len){
    UNIMPLEMENTED();
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
    return content;
}

bool RemoteStorageDevice::StreamWriteStart(FileHandle *handle, const char *path){
    bool rv = RemoteFile::FromPath(&handle->remoteFile, path, FileOpenMode::Write);
    if(!rv) return rv;
    handle->type = RemoteFile::GetType();
    return true;
}

bool RemoteStorageDevice::StreamReadStart(FileHandle *handle, const char *path){
    bool rv = RemoteFile::FromPath(&handle->remoteFile, path, FileOpenMode::Read);
    if(!rv) return rv;
    handle->type = RemoteFile::GetType();
    return true;
}

size_t RemoteStorageDevice::StreamWriteBytes(FileHandle *handle, void *ptr,
                                             size_t size, size_t nmemb)
{
    return handle->remoteFile.WriteBytes(ptr, size, nmemb);
}

size_t RemoteStorageDevice::StreamReadBytes(FileHandle *handle, void *ptr,
                                            size_t size, size_t nmemb)
{
    return handle->remoteFile.ReadBytes(ptr, size, nmemb);
}

bool RemoteStorageDevice::StreamWriteString(FileHandle *handle, const char *str){
    return handle->remoteFile.WriteString(str, 0);
}

bool RemoteStorageDevice::StreamFinish(FileHandle *handle){
    handle->type = -1;
    return handle->remoteFile.CloseFile();
}

bool RemoteStorageDevice::AppendTo(const char *path, const char *str, int with_line_brk){
    return FileAppendTo<RemoteFile>(path, str, with_line_brk);
}

void RemoteStorageDevice::CloseFile(FileHandle *handle){
    handle->remoteFile.CloseFile();
}

//////////////////////////////////////////////////////////////////
//                 R E M O T E   F I L E                        //
//i.e.: Dispatch operations to the RPC handler.                 //
//////////////////////////////////////////////////////////////////
size_t RemoteFile::WriteBytes(void *ptr, size_t size, size_t nmemb){
    // TODO: Implement me!
    UNIMPLEMENTED();
    return 0;
}

size_t RemoteFile::ReadBytes(void *ptr, size_t size, size_t nmemb){
    // TODO: Implement me!
    UNIMPLEMENTED();
    return 0;
}

bool RemoteFile::WriteString(const char *str, int with_line_brk){
    // TODO: Implement me!
    if(!is_open) return true;
    UNIMPLEMENTED();
    return false;
}

bool RemoteFile::CloseFile(){
    // TODO: Implement me!
    if(!is_open) return true;
    UNIMPLEMENTED();
    return true;
}

bool RemoteFile::OpenFile(const char *path, FileOpenMode mode){
    // TODO: Implement me!
    return false;
}

bool RemoteFile::FromPath(RemoteFile *file, const char *path, FileOpenMode mode){
    // TODO: Implement me!
    UNIMPLEMENTED();
    return false;
}

