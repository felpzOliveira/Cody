/* date = June 10th 2021 11:24 */
#if !defined(SERVER_H)
#define SERVER_H
#include <parallel.h>
#include <map>
#include <vector>
#include <rpc_services.h>
#include <security_services.h>

#define MAX_TRANSPORT_SMALL_TIMEOUT_SECONDS 0.005
#define MAX_TRANSPORT_TIMEOUT_SECONDS 0.05
#define MAX_TRANSPORT_LARGE_TIMEOUT_SECONDS 0.2
#define MAX_TRANSPORT_SMALL_TIMEOUT_MS (MAX_TRANSPORT_SMALL_TIMEOUT_SECONDS * 1000.0)
#define MAX_TRANSPORT_TIMEOUT_MS (MAX_TRANSPORT_TIMEOUT_SECONDS * 1000.0)
#define MAX_TRANSPORT_LARGE_TIMEOUT_MS (MAX_TRANSPORT_LARGE_TIMEOUT_SECONDS * 1000.0)
#define MAX_TRANSPORT_SIZE 16384

#if MAX_TRANSPORT_SIZE < 33
    #error "Minimum transport size is 33 ( 2 * 16 + 1 )."
#endif

typedef enum{
    P_NO_ERROR=0,
    READ_TIMEDOUT,
    SEND_FAILURE,
    CONNECTION_CLOSED,
    POLL_FAILURE,
    INVALID_REQUEST,
    ENCRYPT_FAILURE,
    DECRYPT_FAILURE,
    UNKNOWN
}ProtocolError;

// All messages are made up of a code and a raw data, both go through
// encrypted with AES-256.
class RPCMessage{
    public:
    int code;
    std::vector<uint8_t> message;
};

// application thread
class RPCBinder{
    public:
    ConcurrentTimedQueue<RPCMessage> messageQ;
    RPCBinder() = default;
};

class BridgeOpt{
    public:
    std::string ip;
    int port;
    uint8_t *key;

    BridgeOpt() = default;

    BridgeOpt(std::string _ip, int _port, uint8_t *k_ptr){
        ip = _ip;
        port = _port;
        key = k_ptr;
    }
};

// dedicated thread
class RPCServer{
    public:
    SecurityServices::Context ctx_base, ctx_out;
    int mode;
    BridgeOpt bopts;
    RPCNetwork net, net_out;
    RPCBaseCommand *activeCmd;
    std::map<RPCCommandCode, RPCBaseCommand *> cmdMap;

    RPCServer() = default;
    ~RPCServer() = default;

    void Initialize(int md, BridgeOpt b_opts, uint8_t *u_key=nullptr){
        mode = md;
        SecurityServices::CreateContext(ctx_base, u_key);
        if(md == 1){
            bopts = b_opts;
            SecurityServices::CreateContext(ctx_out, b_opts.key);
        }else{
            SetStorageDevice(StorageDeviceType::Local);
        }
    }
    void Start(int port);
    void Terminate();
};

#endif
