#include <server.h>
#include <aes.h>
#include <chrono>
#include <utilities.h>

#define LOG_MODULE "NET"
#include <log.h>

#define LOG_CLIENT(msg) LOG(COLOR_CYAN << "[Client] " << COLOR_NONE << msg)
#define LOG_SERVER(msg) LOG(COLOR_YELLOW_BRIGHT << "[Server] " << COLOR_NONE << msg)

#define MAX_BLOCK_SIZE (MAX_TRANSPORT_SIZE-2*AES_BLOCK_SIZE_IN_BYTES)
#define MIN_BLOCKS(n) ((n + (MAX_BLOCK_SIZE-1)) / MAX_BLOCK_SIZE)

#if defined(_WIN32)
// TODO: Port networking code to windows so we can also do remote stuff there
//       it shouldn't be too hard, but we might want to rework the linux part
//       so we can re-use in windows and not actually re-implement these routines
// TODO: Once this is implemented dont forget to adjust cmake to build server binary!
RPCClient::RPCClient(){
    checkerRunning = false;
    checkerDone = true;
    // TODO
    LOG_ERR("RPCClient not implemented");
}

RPCClient::~RPCClient(){
    // TODO
}

bool RPCClient::ConnectTo(const char *ip, int port){
    // TODO
    LOG_ERR("RPCClient connect not implemented");
    return false;
}

bool RPCClient::ReadEntireFile(std::vector<uint8_t> &out, const char *path){
    // TODO
    LOG_ERR("RPCClient read entire file not implemented");
    return false;
}

bool RPCClient::StreamWriteStart(std::vector<uint8_t> &out, const char *path){
    // TODO
    LOG_ERR("RPCClient stream write start not implemented");
    return false;
}

bool RPCClient::StreamWriteUpdate(std::vector<uint8_t> &out, uint8_t *ptr,
                                  uint32_t size, int mode)
{
    // TODO
    LOG_ERR("RPCClient stream write update not implemented");
    return false;
}

bool RPCClient::StreamWriteFinal(std::vector<uint8_t> &out){
    // TODO
    LOG_ERR("RPCClient stream write final not implemented");
    return false;
}

bool RPCClient::Chdir(std::vector<uint8_t> &out, const char *path){
    // TODO
    LOG_ERR("RPCClient chdir not implemented");
    return false;
}

bool RPCClient::GetPwd(std::vector<uint8_t> &out){
    // TODO
    LOG_ERR("RPCClient getpwd not implemented");
    return false;
}

bool RPCClient::ListFiles(std::vector<uint8_t> &out, const char *path){
    // TODO
    LOG_ERR("RPCClient list files not implemented");
    return false;
}

bool RPCClient::AppendTo(std::vector<uint8_t> &out, const char *path, uint8_t *data,
                         uint32_t size, int with_line_brk)
{
    // TODO
    LOG_ERR("RPCClient append to not implemented");
    return false;
}

void RPCClient::SetSecurityContext(SecurityServices::Context *ctx){
    SecurityServices::CopyContext(&secCtx, ctx);
}

// TODO: Implement also the server routines!
void RPCServer::Start(int port){
    // TODO
    LOG_ERR("RPCServer start not implemented");
}

void RPCServer::Terminate(){
    // TODO
    LOG_ERR("RPCServer terminate not implemented");
}

#else // linux implementation
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
#include <security_services.h>
#include <sys/time.h>
#include <poll.h>

struct NetworkLinux{
    int sockfd;
    int clientfd;
};

int LinuxNetwork_CreateClientSocket(const char *ip, int port);
static bool ClientDoChallenge(SecurityServices::Context *ctx, int socket);

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

static bool SendXBytes(int socket, uint32_t x, void *buffer){
    uint32_t bytes = 0;
    int result = 0;
    uint8_t *uc_ptr = (uint8_t *)buffer;
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

static uint32_t ReadExactlyXBytes(int socket, uint32_t x, void *buffer){
    uint32_t bytes = 0;
    uint8_t *uc_ptr = (uint8_t *)buffer;

    while(bytes < x){
        int read = recv(socket, uc_ptr + bytes, x - bytes, MSG_NOSIGNAL);
        if(read < 1) break;
        bytes += read;
    }

    LOG_VERBOSE(" <<< Received " << bytes << " bytes");
    AssertA(bytes == x, "Unexpected number of bytes");
    return bytes;
}

static uint32_t ReadXBytes(int socket, uint32_t x, void *buffer){
    uint32_t bytes = 0;
    uint8_t *uc_ptr = (uint8_t *)buffer;

    bytes = recv(socket, uc_ptr, x, MSG_NOSIGNAL);
    if(bytes < 1){
        return 0;
    }

    LOG_VERBOSE(" <<< Received " << bytes << " bytes");
    return bytes;
}

static uint32_t ReadUpToXBytesTimed(int socket, uint32_t x, void *buffer,
                                    long timeout, ProtocolError &err)
{
    int ret;
    uint32_t bytes = 0;
    uint8_t *uc_ptr = (uint8_t *)buffer;
    err = ProtocolError::P_NO_ERROR;
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

static ProtocolError EncryptAndSend(SecurityServices::Context *ctx, int socket,
                                    uint8_t *ptr, uint32_t length)
{
    std::vector<uint8_t> enc;
    ProtocolError err = ProtocolError::P_NO_ERROR;
    uint32_t size = 0;
    RPCBuffer buffer;

    LOG_VERBOSE(" - Encrypting " << length);
    if(!SecurityServices::Encrypt(ctx, ptr, length, enc)){
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

static ProtocolError ReadAndDecrypt(SecurityServices::Context *ctx, int socket, uint32_t x,
                                    std::vector<uint8_t> &out, long timeout_ms)
{
    uint32_t memSize = 0;
    uint8_t *memPtr = nullptr;
    ProtocolError err = ProtocolError::P_NO_ERROR;

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

    if(!SecurityServices::Decrypt(ctx, memPtr, memSize, out)){
        err = ProtocolError::DECRYPT_FAILURE;
        goto __finish;
    }

__finish:
    if(memPtr) delete[] memPtr;
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
static bool ServerDoChallenge(SecurityServices::Context *ctx, int socket){
    Challenge ch;
    uint32_t read_size = 0;
    std::vector<uint8_t> vec;
    char mem[64];
    ProtocolError err = ProtocolError::P_NO_ERROR;
    struct timeval start, end;

    AssertA(sizeof(mem) >= SecurityServices::ChallengeExpectedSize(),
            "Challenge size is too big");

    gettimeofday(&start, NULL);

    if(!SecurityServices::CreateChallenge(ctx, vec, ch)){
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

    if(err != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed with: " << GetNetworkError(err));
        return false;
    }

    gettimeofday(&end, NULL);

    double ival = TimevalDifference(start, end);
    if(ival >= MAX_TRANSPORT_TIMEOUT_SECONDS || ival < 0){
        LOG_ERR("Message took to long");
        return false;
    }

    if(!SecurityServices::IsChallengeSolved(ctx, (uint8_t *)mem, read_size, ch)){
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
    NetworkLinux *linuxNetOut = (NetworkLinux *)server->net_out.prv;
    uint32_t code_block_size =
            SecurityServices::PackageRequiredSize(sizeof(RPCCommandCode));

    LOG_VERBOSE("Got new connection");

    if(!ServerDoChallenge(&server->ctx_base, linuxNet->clientfd)){
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

        ProtocolError error = ReadAndDecrypt(&server->ctx_base, linuxNet->clientfd,
                                             code_block_size, out, 0);
        if(error != ProtocolError::P_NO_ERROR){
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
            error = ReadAndDecrypt(&server->ctx_base, linuxNet->clientfd, arg_size,
                                   args, MAX_TRANSPORT_TIMEOUT_MS);
            if(error != ProtocolError::P_NO_ERROR){
                LOG_ERR("Failed to recv args with: " << GetNetworkError(error));
                break;
            }

            if(args.size() < 1 && arg_size > 0){
                LOG_ERR("Missing arguments");
                break;
            }
        }

        if(server->mode == 0){
            if(!ExecuteCommand(cmd, &args, in)){
                LOG_ERR("Command failed, terminating connection");
                break;
            }
        }else{
            error = EncryptAndSend(&server->ctx_out, linuxNetOut->sockfd,
                                   (uint8_t *)&code, sizeof(uint32_t));
            if(error != ProtocolError::P_NO_ERROR){
                LOG_ERR("Failed send with:" << GetNetworkError(error));
                break;
            }

            if(args.size() > 0){
                error = EncryptAndSend(&server->ctx_out, linuxNetOut->sockfd,
                                       args.data(), args.size());
                if(error != ProtocolError::P_NO_ERROR){
                    LOG_ERR("Failed send with:" << GetNetworkError(error));
                    break;
                }
            }

            error = ReadAndDecrypt(&server->ctx_out, linuxNetOut->sockfd, 0, in,
                                   MAX_TRANSPORT_LARGE_TIMEOUT_MS);
        }

        error = EncryptAndSend(&server->ctx_base, linuxNet->clientfd, in.data(), in.size());
        if(error != ProtocolError::P_NO_ERROR){
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
    NetworkLinux *linuxNetOut = (NetworkLinux *)net_out.prv;
    if(linuxNet){
        if(linuxNet->clientfd > 0) close(linuxNet->clientfd);
        if(linuxNet->sockfd > 0) close(linuxNet->sockfd);
    }
    if(linuxNetOut){
        if(linuxNetOut->clientfd > 0) close(linuxNetOut->clientfd);
        if(linuxNetOut->sockfd > 0) close(linuxNetOut->sockfd);
    }
    sleep(2);
}

void RPCServer::Start(int port){
    int clientfd = -1;
    NetworkLinux *linuxNetOut = nullptr;
    NetworkLinux *linuxNet = new NetworkLinux;
    StorageDevice *storage = FetchStorageDevice();
    std::string HOME = std::getenv("HOME") ? std::getenv("HOME") : ".";
    if(!linuxNet){
        LOG_ERR("Failed to allocate resources for networking");
        return;
    }

    RPCInitializeCommandMap(cmdMap);

    ack_size = SecurityServices::PackageRequiredSize(sizeof(uint32_t));

    if(mode != 0){
        LOG_SERVER(" * Initializing server in bridge mode ");
        linuxNetOut = new NetworkLinux;
        linuxNetOut->sockfd = LinuxNetwork_CreateClientSocket(bopts.ip.c_str(),
                                                              bopts.port);
        if(linuxNetOut->sockfd < 0){
            LOG_ERR("Failed to connect to " << bopts.ip << ":" << bopts.port);
            return;
        }

        LOG_SERVER("Connected to " << bopts.ip << ":" << bopts.port);
        if(!ClientDoChallenge(&ctx_out, linuxNetOut->sockfd)){
            close(linuxNetOut->sockfd);
            return;
        }

        net_out.prv = (void *)linuxNetOut;
    }else{
        LOG_SERVER(" * Initializing server ");
    }


    linuxNet->sockfd = LinuxNetwork_CreateServerSocket(port);
    if(linuxNet->sockfd < 0){
        LOG_ERR("Failed to create socket");
        goto err_close;
    }

    LOG_SERVER("Setting " << HOME << " as starting path");

    while(1){
        // if not bridge'd change directory
        if(mode == 0) CODY_IGNORE(storage->Chdir(HOME.c_str()));

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

        LOG_INFO("Connecting to server ... ");
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

static bool ClientDoChallenge(SecurityServices::Context *ctx, int socket){
    uint32_t read_size = 0;
    std::vector<uint8_t> out;
    char mem[64];

    AssertA(sizeof(mem) >= SecurityServices::ChallengeExpectedSize(),
            "Challenge size is too big");

    read_size = ReadXBytes(socket, SecurityServices::ChallengeExpectedSize(), mem);
    if(read_size < 1){
        LOG_ERR("Invalid challenge length");
        return false;
    }

    if(!SecurityServices::SolveChallenge(ctx, (uint8_t *)mem, read_size, out)){
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
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- Chdir");
    uint32_t val = RPC_COMMAND_CHDIR;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(&secCtx, sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, ack_size, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    return out[0] == ACK;
}

bool RPCClient::StreamWriteStart(std::vector<uint8_t> &out, const char *path){
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- WriteStart");
    uint32_t code = RPCStreamedWriteCommand::StreamStartCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    RPCBuffer buffer(work_buffer, sizeof(work_buffer));
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    buffer.Push(&code, sizeof(uint32_t));
    buffer.Push((uint8_t *)path, pathSize+1);

    error = EncryptAndSend(&secCtx, sock, buffer.Data(), buffer.Size());
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, ack_size, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

    if(out.size() < 1){
        LOG_ERR("Got invalid response");
        return false;
    }

    return out[0] == ACK;
}

bool RPCClient::StreamWriteUpdate(std::vector<uint8_t> &out, uint8_t *ptr,
                                  uint32_t size, int mode)
{
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- WriteUpdate");
    uint32_t code = RPCStreamedWriteCommand::StreamUpdateCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    RPCBuffer buffer(size + 2*sizeof(uint32_t) + 1);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;
    bool rv = false;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    buffer.Push(&code, sizeof(uint32_t));
    buffer.Push(&mode, sizeof(uint32_t));
    buffer.Push(ptr, size);

    error = EncryptAndSend(&secCtx, sock, buffer.Data(), buffer.Size());
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        goto __finish;
    }

    if(!filter_incomming) return true;

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
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- WriteFinal");
    uint32_t code = RPCStreamedWriteCommand::StreamFinalCode();
    uint32_t val = RPC_COMMAND_STREAM_WRITE;
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;
    bool rv = false;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = EncryptAndSend(&secCtx, sock, (uint8_t *)&code, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        goto __finish;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        goto __finish;
    }

    if(!filter_incomming) return true;

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
    std::lock_guard<std::mutex> guard(mutex);
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

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(&secCtx, sock, (uint8_t *)buffer.Data(), buffer.Size());
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

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
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- ListFiles");
    uint32_t val = RPC_COMMAND_LIST_FILES;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(&secCtx, sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::P_NO_ERROR){
        printf("Failed request with: %s\n", GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

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
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- GetPwd");
    uint32_t val = RPC_COMMAND_GET_PWD;
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

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
    std::lock_guard<std::mutex> guard(mutex);
    LOG_CLIENT(" -- ReadFile");
    uint32_t val = RPC_COMMAND_READ_FILE;
    uint32_t pathSize = strlen(path);
    NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
    int sock = linuxNet->sockfd;

    ProtocolError error = EncryptAndSend(&secCtx, sock, (uint8_t *)&val, sizeof(uint32_t));
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = EncryptAndSend(&secCtx, sock, (uint8_t *)path, pathSize+1);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed request with: " << GetNetworkError(error));
        return false;
    }

    error = ReadAndDecrypt(&secCtx, sock, 0, out, MAX_TRANSPORT_LARGE_TIMEOUT_MS);
    if(error != ProtocolError::P_NO_ERROR){
        LOG_ERR("Failed read with: " << GetNetworkError(error));
        return false;
    }

    if(!filter_incomming) return true;

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

static void RPCClientChecker(RPCClient *client){
    NetworkLinux *linuxNet = (NetworkLinux *)client->net.prv;
    int sock = linuxNet->sockfd;
    uint32_t val = RPC_COMMAND_PING;
    std::vector<uint8_t> out;
    const long pollTimeout = 60000;
    client->checkerDone = false;

    while(client->checkerRunning){
        out.clear();
        {
            LOG_INFO("Querying Server");
            std::lock_guard<std::mutex> guard(client->mutex);
            ProtocolError error = EncryptAndSend(&client->secCtx, sock, (uint8_t *)&val,
                                                 sizeof(uint32_t));
            if(error != ProtocolError::P_NO_ERROR){
                LOG_ERR("Connection error[1]: " << GetNetworkError(error));
                // TODO: terminate application or warn user and than terminate
                _Exit(0);
            }

            error = ReadAndDecrypt(&client->secCtx, sock, 0, out,
                                    MAX_TRANSPORT_LARGE_TIMEOUT_MS);
            if(error != ProtocolError::P_NO_ERROR){
                LOG_ERR("Connection error[2]: " << GetNetworkError(error));
                // TODO: terminate application or warn user and than terminate
                _Exit(0);
            }

            if(out.size() != 1){
                LOG_ERR("Got invalid response");
                // TODO: terminate application or warn user and than terminate
                _Exit(0);
            }

            if(out[0] != ACK){
                LOG_ERR("Got invalid response, not ACK");
                // TODO: terminate application or warn user and than terminate
                _Exit(0);
            }
        }

        (void) ConditionVariableWait(client->cv, client->waitmutex, pollTimeout,
                                     [&](){ return !client->checkerRunning; });
    }

    client->checkerDone = true;
    LOG_INFO(" -- Terminated");
}

static void StopRPCClientChecker(RPCClient *client){
    const long pollTimeout = 200;
    ConditionVariableTriggerOne(client->cv, client->waitmutex,
                                [&](){ client->checkerRunning = false; });

    while(!client->checkerDone){
        std::this_thread::sleep_for(std::chrono::milliseconds(pollTimeout));
    }
}

bool RPCClient::ConnectTo(const char *ip, int port){
    if(checkerRunning) StopRPCClientChecker(this);

    int sock = LinuxNetwork_CreateClientSocket(ip, port);
    if(sock < 0){
        LOG_ERR("Failed to connect to " << ip << ":" << port);
        return false;
    }

    ack_size = SecurityServices::PackageRequiredSize(sizeof(uint32_t));
    LOG_INFO("Connected to " << ip << ":" << port);
    if(!ClientDoChallenge(&secCtx, sock)){
        close(sock);
        return false;
    }

    NetworkLinux *linuxNet = new NetworkLinux;
    linuxNet->sockfd = sock;
    net.prv = (void *)linuxNet;

    checkerRunning = true;
    std::thread(RPCClientChecker, this).detach();
    return true;
}

void RPCClient::SetSecurityContext(SecurityServices::Context *ctx){
    SecurityServices::CopyContext(&secCtx, ctx);
}

RPCClient::RPCClient(){
    checkerRunning = false;
    checkerDone = true;
}

RPCClient::~RPCClient(){
    if(checkerRunning){
        StopRPCClientChecker(this);
    }

    if(net.prv){
        NetworkLinux *linuxNet = (NetworkLinux *)net.prv;
        if(linuxNet->sockfd > 0) close(linuxNet->sockfd);
        delete linuxNet;
    }
}
#endif
