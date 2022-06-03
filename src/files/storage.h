/* date = May 26th 2022 19:56 */
#pragma once
#include <types.h>
#include <utilities.h>
#include <file_details.h>
#include <rpc.h>


struct RPCNetwork{
    // private data for the actual network layer
    void *prv;
};

class RPCClient{
    public:
    RPCNetwork net;

    RPCClient() = default;
    ~RPCClient();
    bool ConnectTo(const char *ip, int port);
    bool ReadEntireFile(std::vector<uint8_t> &out, const char *path);
    bool StreamWriteStart(std::vector<uint8_t> &out, const char *path);
    bool StreamWriteUpdate(std::vector<uint8_t> &out, uint8_t *ptr,
                           uint32_t size, int mode=0);
    bool StreamWriteFinal(std::vector<uint8_t> &out);
    bool Chdir(std::vector<uint8_t> &out, const char *path);
    bool GetPwd(std::vector<uint8_t> &out);
    bool ListFiles(std::vector<uint8_t> &out, const char *path);
    bool AppendTo(std::vector<uint8_t> &out, const char *path, uint8_t *data,
                  uint32_t size, int with_line_brk);
};

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
    virtual bool StreamWriteStart(FileHandle *handle, const char *path) = 0;
    virtual bool StreamWriteString(FileHandle *handle, const char *str) = 0;
    virtual size_t StreamWriteBytes(FileHandle *handle, void *ptr,
                                    size_t size, size_t nmemb) = 0;
    virtual bool StreamFinish(FileHandle *handle) = 0;
    virtual bool AppendTo(const char *path, const char *str, int with_line_brk=1) = 0;
    virtual void CloseFile(FileHandle *handle) = 0;
    virtual int Chdir(const char *path) = 0;
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
    virtual bool StreamWriteStart(FileHandle *handle, const char *path) override;
    virtual bool StreamWriteString(FileHandle *handle, const char *str) override;
    virtual size_t StreamWriteBytes(FileHandle *handle, void *ptr,
                                    size_t size, size_t nmemb) override;
    virtual bool StreamFinish(FileHandle *handle) override;
    virtual bool AppendTo(const char *path, const char *str, int with_line_brk=1) override;
    virtual void CloseFile(FileHandle *handle) override;
    virtual bool IsLocallyStored() override{ return true; }
    virtual int Chdir(const char *path) override{ return CHDIR(path); }
};

/*
* Remote storage device. Implements support for remote control of files through
* the editor's server implementation.
*/
class RemoteStorageDevice : public StorageDevice{
    public:
    RPCClient client;

    RemoteStorageDevice(const char *ip, int port);

    virtual void GetWorkingDirectory(char *dir, uint len) override;
    virtual int ListFiles(char *basePath, FileEntry **entries,
                          uint *n, uint *size) override;
    virtual char *GetContentsOf(const char *path, uint *size) override;
    virtual bool StreamWriteStart(FileHandle *handle, const char *path) override;
    virtual bool StreamWriteString(FileHandle *handle, const char *str) override;
    virtual size_t StreamWriteBytes(FileHandle *handle, void *ptr,
                                    size_t size, size_t nmemb) override;
    virtual bool StreamFinish(FileHandle *handle) override;
    virtual bool AppendTo(const char *path, const char *str, int with_line_brk=1) override;
    virtual void CloseFile(FileHandle *handle) override;
    virtual bool IsLocallyStored() override{ return false; }
    virtual int Chdir(const char *path) override;
};

/*
* Initializes the backup storage device as a local device. Must be called
* before any other initialization routines (even early ones).
*/
void StorageDeviceEarlyInit();

/*
* Get the application storage device.
*/
StorageDevice *FetchStorageDevice();

/*
* Get the backup storage device.
*/
StorageDevice *FetchBackupStorageDevice();

/*
* Sets the application storage device.
* NOTE: This routine should be called at startup and should never be touched
* again as it could de-synchronize the files and generate a complete mess.
*/
void SetStorageDevice(StorageDeviceType type=StorageDeviceType::Local,
                      const char *ip=nullptr, int port=0);

