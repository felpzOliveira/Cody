#include <rpc.h>
#include <storage.h>

#define LOG_MODULE "RPC"
#include <log.h>

bool RPCBaseCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
        LOG_ERR("Called base RPCCommand, terminating connection");
        return false;
}

bool RPCReadCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    if(!args || size < 1) return false;
    std::string path((char *)args, size);
    uint8_t mem[32];
    uint32_t filesize = 0;
    char *content = GetFileContents(path.c_str(), &filesize);
    LOG_INFO("Called read RPCCommand ( " << path << " ) -- " << filesize);

    out.push_back(ACK);
    memcpy(mem, &filesize, sizeof(uint32_t));
    out.insert(out.end(), &mem[0], &mem[sizeof(uint32_t)]);

    if(filesize > 0 && content){
        uint8_t *ptr = (uint8_t *)content;
        out.insert(out.end(), &ptr[0], &ptr[filesize]);
        out.push_back(0);
    }

    AllocatorFree(content);
    return true;
}

bool RPCStreamedWriteCommand::StreamInit(uint8_t *args, uint32_t size,
                                         std::vector<uint8_t> &out)
{
    // broken request, terminate connection
    if(!args || size < 1) return false;

    if(args[size-1] == 0){
        // the socket layer transmit strings with \0 at the end
        // to make sure everything can be correctly read. However
        // this adds one extra byte to the name preventing concatenation
        // so let's remove it here
        size -= 1;
    }

    targetpath = std::string((char *)args, size);
    filepath = targetpath + std::string(".tmp");

    LOG_INFO("Opening file " << filepath << " with backup");

    uint8_t ret = NACK;
    // 1- grab storage which should be local
    StorageDevice *storage = FetchStorageDevice();
    // 2- close file if it was opened
    ResetFileWrite();
    // 3- open the new path
    if(storage->StreamWriteStart(&file, filepath.c_str())){
        ret = ACK;
        has_error = false;
    }

    out.push_back(ret);
    return true;
}

bool RPCStreamedWriteCommand::StreamUpdate(uint8_t *args, uint32_t size,
                                           std::vector<uint8_t> &out)
{
    // broken request, terminate connection
    if(!args || size < sizeof(uint32_t)) return false;
    // 1- grab storage which should be local
    StorageDevice *storage = FetchStorageDevice();
    // 2- check if it is a string write or byte write
    uint32_t code = 0;
    bool ret = false;
    uint8_t *ptr = &args[sizeof(uint32_t)];
    size -= sizeof(uint32_t);

    memcpy(&code, args, sizeof(uint32_t));
    LOG_INFO("Writing " << size << " bytes to file");
    if(code == 0){ // write string
        char *str = (char *)ptr;
        str[size] = 0;
        ret = storage->StreamWriteString(&file, str);
        if(ret){
            out.push_back(ACK);
        }else{
            out.push_back(NACK);
            has_error = true;
            ResetFileWrite();
        }
    }else if(code == 1){ // write bytes
        uint8_t mem[32];
        uint32_t s = storage->StreamWriteBytes(&file, ptr, 1, size);
        ret = s == size;
        if(ret){
            out.push_back(ACK);
        }else{
            out.push_back(NACK);
            has_error = true;
            ResetFileWrite();
        }
        memcpy(mem, &s, sizeof(uint32_t));
        out.insert(out.end(), &mem[0], &mem[sizeof(uint32_t)]);
    }else{
        // invalid write, break connection
        return false;
    }

    return true;
}

bool RPCStreamedWriteCommand::StreamFinal(uint8_t *args, uint32_t size,
                                          std::vector<uint8_t> &out)
{
    LOG_INFO("Closing file");
    // 1- grab storage which should be local
    StorageDevice *storage = FetchStorageDevice();
    bool ret = storage->StreamFinish(&file);
    if(ret){
        if(!has_error){
            LOG_INFO("Commiting file " << targetpath);
            rename(filepath.c_str(), targetpath.c_str());
        }
        out.push_back(ACK);
    }else{
        out.push_back(NACK);
    }
    has_error = true;
    return true;
}

void RPCStreamedWriteCommand::ResetFileWrite(){
    if(filepath.size() > 0){
        StorageDevice *storage = FetchStorageDevice();
        storage->CloseFile(&file);
        remove(filepath.c_str());
    }
}

