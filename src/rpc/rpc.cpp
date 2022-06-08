#include <rpc.h>
#include <storage.h>

#define LOG_MODULE "RPC"
#include <log.h>

bool RPCPingCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    LOG_INFO("Answering ping request");
    out.push_back(ACK);
    return true;
}

void RPCStreamedWriteCommand::Cleanup(){
    LOG_INFO("Running cleanup");
    ResetFileWrite();
}

bool RPCBaseCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
        LOG_ERR("Called base RPCCommand, terminating connection");
        return false;
}

bool RPCAppendToFileCommand::Execute(uint8_t *args, uint32_t size,
                                     std::vector<uint8_t> &out)
{
    if(!args || size < 1) return false;
    RPCBuffer buffer(args, size);
    StorageDevice *storage = FetchStorageDevice();

    uint32_t clen = 0;
    uint32_t filelen = 0;
    uint32_t mode = 0;
    uint8_t *path = nullptr;
    uint8_t *data = nullptr;

    buffer.Pop(&mode, sizeof(uint32_t));
    buffer.Pop(&filelen, sizeof(uint32_t));

    if(filelen < 1){
        LOG_ERR("Invalid file length during append");
        return false;
    }

    path = buffer.Head();
    buffer.Advance(filelen);

    buffer.Pop(&clen, sizeof(uint32_t));
    if(clen < 1){
        LOG_ERR("Invalid data length during append");
        return false;
    }

    data = buffer.Head();

    std::string pstr((char *)path, filelen);

    data[clen-1] = 0;
    if(storage->AppendTo(pstr.c_str(), (const char *)data, mode)){
        out.push_back(ACK);
    }else{
        out.push_back(NACK);
    }

    return true;
}

bool RPCListFilesCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    if(!args || size < 1) return false;
    std::string path((char *)args, size);
    uint32_t len = 0;
    // push ack and then rewrite in case it is not
    out.push_back(ACK);
    int ret = ListFileEntriesLinear((char *)path.c_str(), out, &len);
    if(ret < 0){
        out[0] = NACK;
        LOG_ERR("Could not query " << path);
    }else{
        LOG_INFO("Queried " << path << " for " << len << " files");
    }

    return true;
}

bool RPCGetPwdCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    char path[PATH_MAX];
    size_t len = PATH_MAX;

    GetCurrentWorkingDirectory(path, len);
    len = strlen(path);
    LOG_INFO("Querying current working directory ( " << path << " )");

    out.push_back(ACK);
    out.insert(out.end(), &path[0], &path[len]);
    return true;
}

bool RPCChdirCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    if(!args || size < 1) return false;
    std::string path((char *)args, size);
    int err = CHDIR(path.c_str());
    if(err != 0){
        LOG_ERR("Chdir to path " << path << " failed");
        out.push_back(NACK);
    }else{
        LOG_INFO("Chdir to path " << path);
        out.push_back(ACK);
    }

    return true;
}

bool RPCReadCommand::Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out){
    if(!args || size < 1) return false;
    std::string path((char *)args, size);
    uint8_t mem[32];
    uint32_t filesize = 0;
    char *content = GetFileContents(path.c_str(), &filesize);
    if(filesize > 0) filesize += 1; // add '\0'

    LOG_INFO("Called read ( " << path << " ) -- " << filesize);

    out.push_back(ACK);
    memcpy(mem, &filesize, sizeof(uint32_t));
    out.insert(out.end(), &mem[0], &mem[sizeof(uint32_t)]);

    if(filesize > 0 && content){
        uint8_t *ptr = (uint8_t *)content;
        out.insert(out.end(), &ptr[0], &ptr[filesize]);
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
    LOG_INFO("Writing " << size << " bytes to file ( Code : " << code << " )");
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

