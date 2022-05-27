#include <storage.h>
#include <utilities.h>

StorageDevice *storageDevice = nullptr;

StorageDevice *FetchStorageDevice(){
    return storageDevice;
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

//////////////////////////////////////////////////////////////////
//                   L O C A L   S T O R A G E                  //
//i.e.: The trivial stuff.                                      //
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

//////////////////////////////////////////////////////////////////
//                 R E M O T E   S T O R A G E                  //
//i.e.: For remote usage.                                       //
//////////////////////////////////////////////////////////////////
RemoteStorageDevice::RemoteStorageDevice(const char *ip, int port){

}

int RemoteStorageDevice::ListFiles(char *basePath, FileEntry **entries,
                                   uint *n, uint *size)
{
    printf("Unimplemented\n");
    return 0;
}

void RemoteStorageDevice::GetWorkingDirectory(char *dir, uint len){
    printf("Unimplemented\n");
}

char *RemoteStorageDevice::GetContentsOf(const char *path, uint *size){
    printf("Unimplemented\n");
    *size = 0;
    return nullptr;
}
