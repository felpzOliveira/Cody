/* date = June 10th 2021 11:24 */
#if !defined(SERVER_H)
#define SERVER_H
#include <parallel.h>

// All messages are made up of a code and a raw data, both go through
// encrypted with AES-256.
class RPCMessage{
    public:
    int code;
    std::vector<uint8_t> message;
};

struct RPCNetwork{
    // private data for the actual network layer
    void *prv;
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

    RPCServer() = default;
    void Start(int port);
};

class RPCClient{
    public:
    std::vector<RPCBinder *> binders;
    RPCNetwork net;

    RPCClient() = default;
    void PushBinder(RPCBinder *binder);
    void ConnectTo(const char *ip, int port);
    void WaitMessage(RPCMessage &message, int timeout);
};

#endif
