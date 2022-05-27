/* date = May 26th 2022 19:56 */
#pragma once
#include <types.h>
#include <utilities.h>

typedef enum{
    Local, Remote
}StorageDeviceType;

/*
* Storage Device represents a storage that can be used with the editor.
* It provides access to disk files and path consultation. It abstracts the concept
* of file reading/writing and help other parts of the code be generic enough
* to handle multiple devices.
*/
class StorageDevice{
    public:

    StorageDevice() = default;
    ~StorageDevice() = default;

    virtual void GetWorkingDirectory(char *dir, uint len) = 0;
    virtual int ListFiles(char *basePath, FileEntry **entries, uint *n, uint *size) = 0;
    virtual char *GetContentsOf(const char *path, uint *size) = 0;
    virtual bool IsLocallyStored() = 0;
};

/*
* Local storage device. Implements the standard way a editor works, i.e.:
* reading and writting directly into disk.
*/
class LocalStorageDevice : public StorageDevice{
    public:

    LocalStorageDevice() = default;
    ~LocalStorageDevice() = default;

    virtual void GetWorkingDirectory(char *dir, uint len) override;
    virtual int ListFiles(char *basePath, FileEntry **entries,
                          uint *n, uint *size) override;
    virtual char *GetContentsOf(const char *path, uint *size) override;
    virtual bool IsLocallyStored() override{ return true; }
};

/*
* Remote storage device. Implements support for remote control of files through
* the editor's server implementation.
*/
class RemoteStorageDevice : public StorageDevice{
    public:

    RemoteStorageDevice(const char *ip, int port);

    virtual void GetWorkingDirectory(char *dir, uint len) override;
    virtual int ListFiles(char *basePath, FileEntry **entries,
                          uint *n, uint *size) override;
    virtual char *GetContentsOf(const char *path, uint *size) override;
    virtual bool IsLocallyStored() override{ return false; }
};

/*
* Get the application storage device.
*/
StorageDevice *FetchStorageDevice();

/*
* Sets the application storage device.
* NOTE: This routine should be called at startup and should never be touched
* again as it could de-synchronize the files and generate a complete mess.
*/
void SetStorageDevice(StorageDeviceType type=StorageDeviceType::Local,
                      const char *ip=nullptr, int port=0);

