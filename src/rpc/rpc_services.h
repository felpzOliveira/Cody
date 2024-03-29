/* date = May 30th 2022 21:30 */
#pragma once
#include <map>
#include <vector>
#include <utilities.h>
#include <storage.h>
#include <file_details.h>

#define MAX_DESCRIPTOR_LENGTH 256

#define RPC_COMMAND_READ_FILE     (1 << 0)
#define RPC_COMMAND_STREAM_WRITE  (1 << 1)
#define RPC_COMMAND_CHDIR         (1 << 2)
#define RPC_COMMAND_GET_PWD       (1 << 3)
#define RPC_COMMAND_LIST_FILES    (1 << 4)
#define RPC_COMMAND_APPEND        (1 << 5)
#define RPC_COMMAND_PING          (1 << 6)
#define RPC_COMMAND_INVALID       (1 << 7)
#define ACK 0x3f
#define NACK 0x4f

typedef uint64_t RPCCommandCode;

class RPCBuffer{
    public:
    uint8_t *mem;
    uint32_t size;
    uint32_t head;
    bool owns;

    RPCBuffer(){
        mem = nullptr;
        head = 0;
        owns = false;
    }

    RPCBuffer(uint32_t length){
        owns = false;
        mem = nullptr;
        Prepare(length);
    }

    RPCBuffer(uint8_t *ptr, uint32_t length){
        mem = ptr;
        size = length;
        head = 0;
        owns = false;
    }

    ~RPCBuffer(){
        if(owns && mem) delete[] mem;
    }

    void Prepare(uint32_t length){
        if(owns && mem) delete[] mem;
        mem = new uint8_t[length];
        size = length;
        head = 0;
        owns = true;
    }

    uint8_t *Data(uint32_t h=0){ return &mem[h]; }
    uint8_t *Head(uint32_t h=0){ return &mem[head]; }
    uint32_t Length(){ return size; }
    uint32_t Size(){ return head; }

    void Advance(uint32_t h){
        AssertA(size - head >= h, "Cannot move");
        head += h;
    }

    void Push(void *ptr, uint32_t length){
        AssertA(size - head >= length, "Cannot fit more data");
        memcpy(&mem[head], ptr, length);
        head += length;
    }

    void Pop(void *ptr, uint32_t length){
        AssertA(size - head >= length, "Cannot pop more data");
        memcpy(ptr, &mem[head], length);
        head += length;
    }
};

class RPCBaseCommand{
    public:

    RPCBaseCommand() = default;
    ~RPCBaseCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out);

    virtual int GetArgumentsSize(){
        return -1;
    }

    virtual void Cleanup(){}
};

class RPCReadCommand : public RPCBaseCommand{
    public:

    RPCReadCommand() = default;
    ~RPCReadCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;

    virtual int GetArgumentsSize() override{
        return MAX_DESCRIPTOR_LENGTH;
    }
};

class RPCStreamedWriteCommand : public RPCBaseCommand{
    public:
    FileHandle file;
    bool has_error = true;
    std::string filepath;
    std::string targetpath;

    RPCStreamedWriteCommand() = default;
    ~RPCStreamedWriteCommand() = default;

    /*
     * Open the given filepath and returns ACK/NACK confirming
     */
    bool StreamInit(uint8_t *args, uint32_t size, std::vector<uint8_t> &out);

    /*
     * Write some data into the file
     */
    bool StreamUpdate(uint8_t *args, uint32_t size, std::vector<uint8_t> &out);

    /*
     * Finalizes the write procedure by copying the backup generated and closing the file.
     */
    bool StreamFinal(uint8_t *args, uint32_t size, std::vector<uint8_t> &out);

    /*
     * Resets the write procedure whenever a failure happens.
     */
    void ResetFileWrite();

    /*
     * Cleanup routine for when connection is lost/broken.
     */
    virtual void Cleanup() override;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override{
        uint32_t step = 0;
        uint8_t *ptr = args;
        bool rv = false;
        if(!args || size < sizeof(uint32_t)) return rv;
        memcpy(&step, args, sizeof(uint32_t));

        size -= sizeof(uint32_t);
        ptr = &args[sizeof(uint32_t)];

        if(step == StreamStartCode()){
            rv = StreamInit(ptr, size, out);
        }else if(step == StreamUpdateCode()){
            rv = StreamUpdate(ptr, size, out);
        }else if(step == StreamFinalCode()){
            rv = StreamFinal(ptr, size, out);
        }

        return rv;
    }

    virtual int GetArgumentsSize() override{
        return 0;
    }

    static uint32_t StreamStartCode(){ return 0; }
    static uint32_t StreamUpdateCode(){ return 1; }
    static uint32_t StreamFinalCode(){ return 2; }
};

class RPCGetPwdCommand : public RPCBaseCommand{
    public:

    RPCGetPwdCommand() = default;
    ~RPCGetPwdCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;
};

class RPCListFilesCommand : public RPCBaseCommand{
    public:

    RPCListFilesCommand() = default;
    ~RPCListFilesCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;

    virtual int GetArgumentsSize() override{
        return MAX_DESCRIPTOR_LENGTH;
    }
};

class RPCAppendToFileCommand : public RPCBaseCommand{
    public:

    RPCAppendToFileCommand() = default;
    ~RPCAppendToFileCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;

    virtual int GetArgumentsSize() override{
        return 0;
    }
};

class RPCChdirCommand : public RPCBaseCommand{
    public:

    RPCChdirCommand() = default;
    ~RPCChdirCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;

    virtual int GetArgumentsSize() override{
        return MAX_DESCRIPTOR_LENGTH;
    }
};

class RPCPingCommand : public RPCBaseCommand{
    public:

    RPCPingCommand() = default;
    ~RPCPingCommand() = default;

    virtual bool Execute(uint8_t *args, uint32_t size, std::vector<uint8_t> &out) override;
    virtual int GetArgumentsSize() override{
        return -1;
    }
};

inline
void RPCInitializeCommandMap(std::map<RPCCommandCode, RPCBaseCommand *> &cmdMap){
    cmdMap[RPC_COMMAND_READ_FILE] = new RPCReadCommand();
    cmdMap[RPC_COMMAND_STREAM_WRITE] = new RPCStreamedWriteCommand();
    cmdMap[RPC_COMMAND_CHDIR] = new RPCChdirCommand();
    cmdMap[RPC_COMMAND_GET_PWD] = new RPCGetPwdCommand();
    cmdMap[RPC_COMMAND_LIST_FILES] = new RPCListFilesCommand();
    cmdMap[RPC_COMMAND_APPEND] = new RPCAppendToFileCommand();
    cmdMap[RPC_COMMAND_PING] = new RPCPingCommand();
}
