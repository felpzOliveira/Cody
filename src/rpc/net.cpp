#include <server.h>
#include <aes.h>

#define LOG_MODULE "NET"
#include <log.h>

#define LOG_CLIENT(msg) LOG(COLOR_CYAN << "[Client] " << COLOR_NONE << msg)
#define LOG_SERVER(msg) LOG(COLOR_YELLOW_BRIGHT << "[Server] " << COLOR_NONE << msg)

#define MAX_BLOCK_SIZE (MAX_TRANSPORT_SIZE-2*AES_BLOCK_SIZE_IN_BYTES)
#define MIN_BLOCKS(n) ((n + (MAX_BLOCK_SIZE-1)) / MAX_BLOCK_SIZE)

/*
 * Buffer for RPC stuff.
 */
static uint8_t work_buffer[MAX_TRANSPORT_SIZE];
static uint32_t ack_size = 0;

const char *GetNetworkError(ProtocolError err){
    switch(err){
        case READ_TIMEDOUT: return "Read operation timed out";
        case CONNECTION_CLOSED: return "Connection closed";
        case POLL_FAILURE: return "Socket failure: could not poll";
        case SEND_FAILURE: return "Socket failure: could not send";
        case INVALID_REQUEST: return "The operation is not valid";
        case ENCRYPT_FAILURE: return "Failed to encrypt data";
        case DECRYPT_FAILURE: return "Failed to decrypt data";
        default:{
            return strerror(errno);
        }
    }
}

////////////////////////////////////////////////////////////
//                    S E R V E R                         //
////////////////////////////////////////////////////////////
// Linux
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <security.h>
#include <sys/time.h>
#include <poll.h>

struct NetworkLinux{
    int sockfd;
    int clientfd;
};

void LinuxNetwork_InitHints(struct addrinfo *hints){
    memset(hints, 0x0, sizeof(struct addrinfo));
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_protocol = IPPROTO_TCP;
    hints->ai_flags = AI_PASSIVE;
}

std::string IntToString(int val){
    std::stringstream ss;
    ss << val;
    return ss.str();
}

int LinuxNetwork_CreateServerSocket(int port){
    struct addrinfo hints;
    struct addrinfo *results = nullptr;
    int sock = -1;
    int option = 1;
    std::string str_port = IntToString(port);

    LinuxNetwork_InitHints(&hints);
    int rv = getaddrinfo("0.0.0.0", str_port.c_str(), &hints, &results);
    if(rv != 0){
        LOG_ERR("Could not get address");
        return -1;
    }

    sock = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    if(sock == -1){
        LOG_ERR("Failed to create socket");
        goto err_close;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    rv = bind(sock, results->ai_addr, (int)results->ai_addrlen);
    if(rv != 0){
        LOG_ERR("Failed bind {" << rv << "} ( " <<  strerror(errno) << " )\n");
        goto err_close;
    }

    rv = listen(sock, 100);
    if(rv != 0){
        LOG_ERR("Failed listen {" << rv << "}");
        goto err_close;
    }

    freeaddrinfo(results);
    return sock;

err_close:
    freeaddrinfo(results);
    if(sock > 0) close(sock);
    return -1;
}

static bool SendXBytes(int socket, unsigned int x, void *buffer){
    unsigned int bytes = 0;
    int result = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;
    while(bytes < x){
        result = send(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
        if(result < 1){
            LOG_ERR("Failed send with : " << strerror(errno));
            return false;
        }

        LOG_VERBOSE(" >>> Sent " << result << " bytes");
        bytes += result;
    }
    return true;
}

#if TRANSMITION_LOOP != 0
static unsigned int ReadXBytes(int socket, unsigned int x, void *buffer){
    unsigned int bytes = 0;
    int result = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;
    while(bytes < x){
        LOG_VERBOSE(" -- Waiting for " << (x - bytes));
        result = recv(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
        if(result < 1){
            break;
        }

        LOG_VERBOSE(" <<< Received " << result << " bytes");

        bytes += result;
    }

    return bytes;
}

static unsigned int ReadUpToXBytesTimed(int socket, unsigned int x,
                                        void *buffer, long timeout,
                                        ProtocolError &err)
{
    return ReadXBytes(socket, x, buffer);
    unsigned int bytes = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;
    err = ProtocolError::NO_ERROR;
    while(bytes < x){
        struct pollfd fd;
        int ret;

        fd.fd = socket;
        fd.events = POLLIN;
        ret = poll(&fd, 1, timeout);
        switch(ret){
            case -1:{
                err = ProtocolError::POLL_FAILURE;
                bytes = 0;
                goto __finish;
            } break;
            case 0:{
                goto __finish;
            } break;
            default:{
                int result = recv(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
                if(result < 1){
                    goto __finish;
                }
                LOG_VERBOSE(" <<< Received " << result << " bytes");
                bytes += result;
            } break;
        }

        /*
         * Allow first package to be slow but not others.
         */
        if(timeout > MAX_TRANSPORT_SMALL_TIMEOUT_MS)
            timeout = MAX_TRANSPORT_SMALL_TIMEOUT_MS;
    }
__finish:
    return bytes;
}
#else

static unsigned int ReadExactlyXBytes(int socket, unsigned int x, void *buffer){
    unsigned int bytes = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;
    while(bytes < x){
        int read = recv(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
        if(read < 1) break;
        bytes += read;
    }

    LOG_VERBOSE(" <<< Received " << bytes << " bytes");
    AssertA(bytes == x, "Unexpected number of bytes");
    return bytes;
}

static unsigned int ReadXBytes(int socket, unsigned int x, void *buffer){
    unsigned int bytes = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;

    bytes = recv(socket, uc_ptr, x, MSG_NOSIGNAL);
    if(bytes < 1){
        return 0;
    }

    LOG_VERBOSE(" <<< Received " << bytes << " bytes");
    //AssertA(bytes == x, "Unexpected number of bytes");
    return bytes;
}

static unsigned int ReadUpToXBytesTimed(int socket, unsigned int x,
                                        void *buffer, long timeout,
                                        ProtocolError &err)
{
    return ReadXBytes(socket, x, buffer);
    int ret;
    unsigned int bytes = 0;
    unsigned char *uc_ptr = (unsigned char *)buffer;
    err = ProtocolError::NO_ERROR;
    struct pollfd fd;

    fd.fd = socket;
    fd.events = POLLIN;
    ret = poll(&fd, 1, timeout);
    switch(ret){
        case -1:{
            err = ProtocolError::POLL_FAILURE;
            bytes = 0;
            goto __finish;
        } break;
        case 0:{
            goto __finish;
        } break;
        default:{
            int result = recv(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
            if(result < 1){
                goto __finish;
            }
            LOG_VERBOSE(" <<< Received " << result << " bytes");
            bytes += result;
        } break;
    }
__finish:
    return bytes;
}
#endif

static ProtocolError EncryptAndSend(int socket, uint8_t *ptr, uint32_t length){
    std::vector<uint8_t> enc;
    ProtocolError err = ProtocolError::NO_ERROR;
    uint32_t size = 0;
    RPCBuffer buffer;

    LOG_VERBOSE(" - Encrypting " << length);
    if(!SecurityServices::Encrypt(ptr, length, enc)){
        err = ProtocolError::ENCRYPT_FAILURE;
        goto __finish;
    }

    size = enc.size();
    buffer.Prepare(size + sizeof(uint32_t));
    buffer.Push(&size, sizeof(uint32_t));
    buffer.Push(enc.data(), size);

    LOG_VERBOSE(" - Sending " << buffer.Size());
    if(!SendXBytes(socket, buffer.Size(), buffer.Data())){
        err = ProtocolError::SEND_FAILURE;
        goto __finish;
    }
__finish:
    return err;
}

static ProtocolError ReadAndDecrypt(int socket, uint32_t x, std::vector<uint8_t> &out,
                                    long timeout_ms)
{
    uint32_t memSize = 0;
    uint8_t *memPtr = nullptr;
    ProtocolError err = ProtocolError::NO_ERROR;

    uint32_t read_size = ReadXBytes(socket, sizeof(uint32_t), &memSize);
    if(read_size != sizeof(uint32_t)){
        LOG_ERR("Did not read the size of the package");
        err = ProtocolError::INVALID_REQUEST;
        goto __finish;
    }

    memPtr = new uint8_t[memSize];
    if(!memPtr){
        LOG_ERR("Could not allocate memory for " << memSize << " bytes");
        err = ProtocolError::INVALID_REQUEST;
        goto __finish;
    }

    LOG_VERBOSE(" - Waiting for " << memSize << " bytes");
    read_size = ReadExactlyXBytes(socket, memSize, memPtr);
    if(read_size != memSize){
        LOG_ERR("Did not read package entirely");
        err = ProtocolError::INVALID_REQUEST;
        goto __finish;
    }

    if(!SecurityServices::Decrypt(memPtr, memSize, out)){
        err = ProtocolError::DECRYPT_FAILURE;
        goto __finish;
    }

__finish:
    if(memPtr) delete[] memPtr;
    return err;
}

static ProtocolError EncryptAndSend2(int socket, uint8_t *ptr, uint32_t remaining){
    std::vector<uint8_t> enc;
    ProtocolError err = ProtocolError::NO_ERROR;
    uint32_t totalSize = remaining;
    uint32_t blocks = MIN_BLOCKS(remaining);
    uint32_t blen = 0;

    LOG_VERBOSE("Sending data in " << blocks << " blocks ( " << remaining << " )");

    // 1- Send the amount of blocks to transmit
    if(!SecurityServices::Encrypt((uint8_t *)&blocks, sizeof(uint32_t), enc)){
        err = ProtocolError::ENCRYPT_FAILURE;
        goto __finish;
    }

    if(!SendXBytes(socket, enc.size(), enc.data())){
        err = ProtocolError::SEND_FAILURE;
        goto __finish;
    }

    // 2- Send the blocks, untill we have init/begin/finish on crypto, encrypt each block
    for(uint32_t i = 0; i < blocks; i++){
        enc.clear();
        blen = remaining > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : remaining;

        if(!SecurityServices::Encrypt(&ptr[totalSize-remaining], blen, enc)){
            err = ProtocolError::ENCRYPT_FAILURE;
            goto __finish;
        }

        if(blen == MAX_BLOCK_SIZE){
            if(enc.size() != MAX_TRANSPORT_SIZE){
                LOG_ERR("Encrypted size is " << enc.size());
            }
            AssertA(enc.size() == MAX_TRANSPORT_SIZE,
                    "Encrypted size is not max package size");
        }

        if(!SendXBytes(socket, enc.size(), enc.data())){
            err = ProtocolError::SEND_FAILURE;
            goto __finish;
        }

        remaining -= blen;
    }

__finish:
    return err;
}

static ProtocolError ReadAndDecrypt2(int socket, uint32_t x, std::vector<uint8_t> &out,
                                    long timeout_ms)
{
    std::vector<uint8_t> tmp;
    uint32_t read_size = 0;
    uint32_t blocks = 0;
    bool is_limited = x > 0;
    ProtocolError err = ProtocolError::NO_ERROR;
    uint32_t block_count_size = SecurityServices::PackageRequiredSize(sizeof(uint32_t));

    if(timeout_ms == 0)
        read_size = ReadXBytes(socket, block_count_size, work_buffer);
    else
        read_size = ReadUpToXBytesTimed(socket, block_count_size, work_buffer,
                                        timeout_ms, err);

    if(read_size < 1 && err == ProtocolError::NO_ERROR){
        err = ProtocolError::CONNECTION_CLOSED;
        goto __finish;
    }

    if(err != ProtocolError::NO_ERROR){
        goto __finish;
    }

    if(!SecurityServices::Decrypt(work_buffer, read_size, tmp)){
        err = ProtocolError::DECRYPT_FAILURE;
        goto __finish;
    }

    if(tmp.size() != sizeof(uint32_t)){
        err = ProtocolError::INVALID_REQUEST;
        goto __finish;
    }

    memcpy(&blocks, tmp.data(), sizeof(uint32_t));

    LOG_VERBOSE("Reading " << blocks << " blocks");

    for(uint32_t i = 0; i < blocks; i++){
        uint32_t to_read_size = MAX_TRANSPORT_SIZE;
        if(is_limited){
            to_read_size = x > MAX_TRANSPORT_SIZE ? MAX_TRANSPORT_SIZE : x;
        }

        read_size = ReadUpToXBytesTimed(socket, to_read_size, work_buffer,
                                        MAX_TRANSPORT_TIMEOUT_MS, err);

        if(read_size < 1 && err == ProtocolError::NO_ERROR){
            err = ProtocolError::CONNECTION_CLOSED;
            goto __finish;
        }

        if(err != ProtocolError::NO_ERROR){
            goto __finish;
        }

        if(!SecurityServices::Decrypt(work_buffer, read_size, out)){
            err = ProtocolError::DECRYPT_FAILURE;
            goto __finish;
        }

        if(is_limited){
            x -= to_read_size;
            if(x == 0) break;
        }
    }

__finish:
    return err;
}

static double TimevalDifference(struct timeval start, struct timeval end){
    double taken = (end.tv_sec - start.tv_sec) * 1e6;
    taken = (taken + (end.tv_usec - start.tv_usec)) * 1e-6;
    return taken;
}

// TODO: Add a table lookup for errors x time
//       so we can prevent multiple connections trying
//       to brute-force the key
static bool ServerDoChallenge(int socket){
    Challenge ch;
    unsigned int read_size = 0;
    std::vector<uint8_t> vec;
    char mem[64];
    ProtocolError err = ProtocolError::NO_ERROR;
    struct timeval start, end;

    AssertA(sizeof(mem) >= SecurityServices::ChallengeExpectedSize(),
            "Challenge size is too big");

    gettimeofday(&start, NULL);

    if(!SecurityServices::CreateChallenge(vec, ch)){
        LOG_ERR("Failed to create challenge");
        return false;
    }

    if(!SendXBytes(socket, vec.size(), vec.data())){
        LOG_ERR("Could not send challenge");
        return false;
    }

    read_size = ReadUpToXBytesTimed(socket, SecurityServices::ChallengeExpectedSize(),
                                    mem, MAX_TRANSPORT_TIMEOUT_MS, err);
    if(read_size < 1){
        LOG_ERR("Did not receive proper response");
        return false;
    }

    if(err != ProtocolError::NO_ERROR){
        LOG_ERR("Failed with: " << GetNetworkError(err));
        return false;
    }

    gettimeofday(&end, NULL);

    double ival = TimevalDifference(start, end);
    if(ival >= MAX_TRANSPORT_TIMEOUT_SECONDS || ival < 0){
        LOG_ERR("Message took to long");
        return false;
    }

    if(!SecurityServices::IsChallengeSolved((uint8_t *)mem, read_size, ch)){
        LOG_ERR("Invalid response for challenge");
        mem[0] = NACK;
        SendXBytes(socket, 1, mem);
        return false;
    }

    mem[0] = ACK;
    if(!SendXBytes(socket, 1, mem)){
        LOG_ERR("Failed to send ack byte");
        return false;
    }

    LOG_INFO("Correctly solved challenge in " << ival);
    return true;
}

bool ExecuteCommand(RPCBaseCommand *cmd, std::vector<uint8_t> *args,
                    std::vector<uint8_t> &out)
{
    return cmd->Execute(args->data(), args->size(), out);
}

void ServerService(RPCServer *server){
    NetworkLinux *linuxNet = (NetworkLinux *)server->net.prv;
    unsigned int code_block_size =
            SecurityServices::PackageRequiredSize(sizeof(RPCCommandCode));

    LOG_VERBOSE("Got new connection");

    if(!ServerDoChallenge(linuxNet->clientfd)){
        close(linuxNet->clientfd);
        return;
    }

    // If the challenge was OK, do command loop
    std::vector<uint8_t> out, in, args;
    while(1){
        RPCCommandCode code = RPC_COMMAND_INVALID;
        in.clear();
        out.clear();
        args.clear();

        ProtocolError error = ReadAndDecrypt(linuxNet->clientfd, code_block_size, out, 0);
        if(error != ProtocolError::NO_ERROR){
            LOG_ERR("Failed recv with:" << GetNetworkError(error));
            break;
        }

        code = out[0];

        LOG_VERBOSE("Got code " << code << " ( " << out.size() << " )");
        out.clear();
        // find command and check for args
        if(server->cmdMap.find(code) == server->cmdMap.end()){
            LOG_ERR("Invalid command");
            break;
        }

        RPCBaseCommand *cmd = server->cmdMap[code];
        int arg_size = cmd->GetArgumentsSize();
        server->activeCmd = cmd;

        if(arg_size >= 0){
            error = ReadAndDecrypt(linuxNet->clientfd, arg_size,
                                   args, MAX_TRANSPORT_TIMEOUT_MS);
            if(error != ProtocolError::NO_ERROR){
                LOG_ERR("Failed to recv args with: " << GetNetworkError(error));
                break;
            }

            if(args.size() < 1 && arg_size > 0){
                LOG_ERR("Missing arguments");
                break;
            }
        }

        if(!ExecuteCommand(cmd, &args, in)){
            LOG_ERR("Command failed, terminating connection");
            break;
        }

        error = EncryptAndSend(linuxNet->clientfd, in.data(), in.size());
        if(error != ProtocolError::NO_ERROR){
            LOG_ERR("Failed send with: " << GetNetworkError(error));
            break;
        }

        server->activeCmd = nullptr;
    }
    close(linuxNet->clientfd);
    linuxNet->clientfd = -1;

    if(server->activeCmd){
        server->activeCmd->Cleanup();
        server->activeCmd = nullptr;
    }
}

void RPCServer::Terminate(){
    LOG_INFO("Terminating...\n");
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    if(linuxNet->clientfd > 0) close(linuxNet->clientfd);
    if(linuxNet->sockfd > 0) close(linuxNet->sockfd);
    sleep(2);
}

void RPCServer::Start(int port){
    int clientfd = -1;
    NetworkLinux *linuxNet = new NetworkLinux;
    StorageDevice *storage = FetchStorageDevice();
    std::string HOME = std::getenv("HOME") ? std::getenv("HOME") : ".";
    if(!linuxNet){
        LOG_ERR("Failed to allocate resources for networking");
        return;
    }

    RPCInitializeCommandMap(cmdMap);

    ack_size = SecurityServices::PackageRequiredSize(sizeof(uint32_t));

    linuxNet->sockfd = LinuxNetwork_CreateServerSocket(port);
    if(linuxNet->sockfd < 0){
        LOG_ERR("Failed to create socket");
        goto err_close;
    }

    LOG_SERVER("Setting " << HOME << " as starting path");

    while(1){
        IGNORE(storage->Chdir(HOME.c_str()));
        linuxNet->clientfd = -1;
        LOG_SERVER("Waiting for connection on port " << port);

        clientfd = accept(linuxNet->sockfd, NULL, NULL);
        if(clientfd < 0){
            LOG_ERR("Failed to accept client");
            goto err_close;
        }

        linuxNet->clientfd = clientfd;
        net.prv = (void *)linuxNet;

        ServerService(this);
    }
err_close:
    delete linuxNet;
}

////////////////////////////////////////////////////////////
//                    C L I E N T                         //
////////////////////////////////////////////////////////////

int LinuxNetwork_CreateClientSocket(const char *ip, int port){
    struct addrinfo hints;
    struct addrinfo *ref = nullptr;
    struct addrinfo *results = nullptr;
    int sock = -1;
    std::string str_port = IntToString(port);

    LinuxNetwork_InitHints(&hints);
    hints.ai_family = AF_UNSPEC;

    int rv = getaddrinfo(ip, str_port.c_str(), &hints, &results);
    if(rv != 0){
        LOG_ERR("Could not get address");
        return -1;
    }

    for(ref = results; ref != NULL; ref = ref->ai_next){
        sock = socket(ref->ai_family, ref->ai_socktype, ref->ai_protocol);
        if(sock == -1){
            LOG_ERR("Failed to create socket");
            goto err_close;
        }

        rv = connect(sock, ref->ai_addr, (int)ref->ai_addrlen);
        if(rv != 0){
            LOG_ERR("Failed to connect to target");
            goto err_close;
        }
    }

    freeaddrinfo(results);
    return sock;

err_close:
    freeaddrinfo(results);
    if(sock > 0) close(sock);
    return sock;
}

static bool ClientDoChallenge(int socket){
    unsigned int read_size = 0;
    std::vector<uint8_t> out;
    char mem[64];

    AssertA(sizeof(mem) >= SecurityServices::ChallengeExpectedSize(),
            "Challenge size is too big");

    read_size = ReadXBytes(socket, SecurityServices::ChallengeExpectedSize(), mem);
    if(read_size < 1){
        LOG_ERR("Invalid challenge length");
        return false;
    }

    if(!SecurityServices::SolveChallenge((uint8_t *)mem, read_size, out)){
        LOG_ERR("Could not solve challenge");
        return false;
    }

    if(!SendXBytes(socket, out.size(), out.data())){
        LOG_ERR("Failed to send challenge response");
        return false;
    }

    read_size = ReadXBytes(socket, 1, mem);
    if(read_size != 1){
        LOG_ERR("Could not read ack byte");
        return false;
    }

    if(mem[0] == NACK){
        LOG_ERR("Failed challenge");
        return false;
    }else if(mem[0] == ACK){
        LOG_VERBOSE("Challenge succeded");
        return true;
    }else{
        LOG_ERR("Unknown response " << mem[0]);
        return false;
    }
}

bool RPCClient::Chdir(std::vector<uint8_t> &out, const char *path){
    LOG_CLIENT(" -- Chdir");
    uint32_t val = RPC_COMMAND_CHDIR;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, ack_size, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    return out[0] == ACK;
}

bool RPCClient::StreamWriteStart(std::vector<uint8_t> &out, const char *path){
    LOG_CLIENT(" -- WriteStart");
    uint32_t code = RPCStreamedWriteCommand::StreamStartCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    RPCBuffer buffer(work_buffer, sizeof(work_buffer));
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    buffer.Push(&code, sizeof(uint32_t));
    buffer.Push((uint8_t *)path, pathSize+1);

    error = EncryptAndSend(sock, buffer.Data(), buffer.Size());
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, ack_size, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    return out[0] == ACK;
}

bool RPCClient::StreamWriteUpdate(std::vector<uint8_t> &out, uint8_t *ptr,
                                  uint32_t size, int mode)
{
    LOG_CLIENT(" -- WriteUpdate");
    uint32_t code = RPCStreamedWriteCommand::StreamUpdateCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    RPCBuffer buffer(size + 2*sizeof(uint32_t) + 1);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;
    bool rv = false;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    buffer.Push(&code, sizeof(uint32_t));
    buffer.Push(&mode, sizeof(uint32_t));
    buffer.Push(ptr, size);

    error = EncryptAndSend(sock, buffer.Data(), buffer.Size());
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        goto __finish;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        goto __finish;
    }

    val = out[0];

    out.erase(out.begin(), out.begin()+1);

    rv = val == ACK;
__finish:
    return rv;
}

bool RPCClient::StreamWriteFinal(std::vector<uint8_t> &out){
    LOG_CLIENT(" -- WriteFinal");
    uint32_t code = RPCStreamedWriteCommand::StreamFinalCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;
    bool rv = false;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = EncryptAndSend(sock, (uint8_t *)&code, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        goto __finish;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        goto __finish;
    }

    val = out[0];

    rv = val == ACK;
__finish:
    return rv;
}

bool RPCClient::AppendTo(std::vector<uint8_t> &out, const char *path, uint8_t *data,
                         uint32_t size, int with_line_brk)
{
    LOG_CLIENT(" -- AppendTo");
    uint32_t val = RPC_COMMAND_APPEND;
    uint32_t pathSize = strlen(path)+1;
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    RPCBuffer buffer(3 * sizeof(uint32_t) + size + pathSize);
    buffer.Push(&with_line_brk, sizeof(uint32_t));
    buffer.Push(&pathSize, sizeof(uint32_t));
    buffer.Push((uint8_t *)path, pathSize);
    buffer.Push(&size, sizeof(uint32_t));
    buffer.Push(data, size);

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(sock, (uint8_t *)buffer.Data(), buffer.Size());
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    if(out[0] == ACK){
        return true;
    }

    return false;
}

bool RPCClient::ListFiles(std::vector<uint8_t> &out, const char *path){
    LOG_CLIENT(" -- ListFiles");
    uint32_t val = RPC_COMMAND_LIST_FILES;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    if(out[0] == ACK){
        out.erase(out.begin(), out.begin() + 1);
        return true;
    }

    return false;
}

bool RPCClient::GetPwd(std::vector<uint8_t> &out){
    LOG_CLIENT(" -- GetPwd");
    uint32_t val = RPC_COMMAND_GET_PWD;
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    if(out[0] == ACK){
        out.erase(out.begin(), out.begin() + 1);
        return true;
    }

    return false;
}

bool RPCClient::ReadEntireFile(std::vector<uint8_t> &out, const char *path){
    LOG_CLIENT(" -- ReadFile");
    uint32_t val = RPC_COMMAND_READ_FILE;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    uint8_t *ptr = out.data();
    val = ptr[0];
    if(val == ACK && out.size() >= sizeof(uint32_t)+1){
        memcpy(&val, &ptr[1], sizeof(uint32_t));
        if(val > 0){
            out.erase(out.begin(), out.begin() + (sizeof(uint32_t)+1));
        }

        return true;
    }

    return false;
}

bool RPCClient::ConnectTo(const char *ip, int port){
    int sock = LinuxNetwork_CreateClientSocket(ip, port);
    if(sock < 0){
        LOG_ERR("Failed to connect to " << ip << ":" << port);
        return false;
    }

    ack_size = SecurityServices::PackageRequiredSize(sizeof(uint32_t));
    LOG_INFO("Connected [ " << sock << " ]");
    if(!ClientDoChallenge(sock)){
        close(sock);
        return false;
    }

    NetworkLinux *linuxNet = new NetworkLinux;
    linuxNet->sockfd = sock;
    net.prv = (void *)linuxNet;
    return true;
}

RPCClient::~RPCClient(){
    if(net.prv){
        NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
        if(linuxNet->sockfd > 0) close(linuxNet->sockfd);
        delete linuxNet;
    }
}

