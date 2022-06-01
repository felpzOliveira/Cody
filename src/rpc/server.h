/* date = June 10th 2021 11:24 */
#if !defined(SERVER_H)
#define SERVER_H
#include <parallel.h>
#include <map>
#include <vector>
#include <rpc.h>

#define MAX_TRANSPORT_TIMEOUT_SECONDS 0.05
#define MAX_TRANSPORT_LARGE_TIMEOUT_SECONDS 0.2
#define MAX_TRANSPORT_TIMEOUT_MS (MAX_TRANSPORT_TIMEOUT_SECONDS * 1000.0)
#define MAX_TRANSPORT_LARGE_TIMEOUT_MS (MAX_TRANSPORT_LARGE_TIMEOUT_SECONDS * 1000.0)
#define MAX_TRANSPORT_SIZE 4096

#if MAX_TRANSPORT_SIZE < 33
    #error "Minimum transport size is 33 ( 2 * 16 + 1 )."
#endif

typedef enum{
    NO_ERROR=0,
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

// dedicated thread
class RPCServer{
    public:
    RPCNetwork net;
    std::map<RPCCommandCode, RPCBaseCommand *> cmdMap;

    RPCServer() = default;
    ~RPCServer() = default;
    void Start(int port);
    void Terminate();
};

#endif
