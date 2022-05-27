#include <server.h>

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
        printf("{NET} Could not get address\n");
        return -1;
    }

    for(ref = results; ref != NULL; ref = ref->ai_next){
        sock = socket(ref->ai_family, ref->ai_socktype, ref->ai_protocol);
        if(sock == -1){
            printf("{NET} Failed to create socket\n");
            goto err_close;
        }

        rv = connect(sock, ref->ai_addr, (int)ref->ai_addrlen);
        if(rv != 0){
            printf("{NET} Failed to connect to target\n");
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

int LinuxNetwork_CreateServerSocket(int port){
    struct addrinfo hints;
    struct addrinfo *results = nullptr;
    int sock = -1;
    std::string str_port = IntToString(port);

    LinuxNetwork_InitHints(&hints);
    int rv = getaddrinfo("0.0.0.0", str_port.c_str(), &hints, &results);
    if(rv != 0){
        printf("{NET} Could not get address\n");
        return -1;
    }

    sock = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
    if(sock == -1){
        printf("{NET} Failed to create socket\n");
        goto err_close;
    }

    rv = bind(sock, results->ai_addr, (int)results->ai_addrlen);
    if(rv != 0){
        printf("{NET} Failed bind {%x}\n", rv);
        goto err_close;
    }

    rv = listen(sock, 100);
    if(rv != 0){
        printf("{NET} Failed listen {%x}\n", rv);
        goto err_close;
    }

    freeaddrinfo(results);
    return sock;

err_close:
    freeaddrinfo(results);
    if(sock > 0) close(sock);
    return -1;
}

void ServerService(RPCServer *server){
    NetworkLinux *linuxNet = (NetworkLinux *)server->net.prv;
    printf("Got connection\n");
    close(linuxNet->clientfd);
    close(linuxNet->sockfd);
}

void RPCServer::Start(int port){
    int clientfd = -1;
    NetworkLinux *linuxNet = new NetworkLinux;
    if(!linuxNet){
        printf("{NET} Failed to allocate resources for networking\n");
        return;
    }

    linuxNet->sockfd = LinuxNetwork_CreateServerSocket(port);
    if(linuxNet->sockfd < 0){
        printf("{NET} Failed to create socket\n");
        goto err_close;
    }

    printf("Waiting for connection on %d\n", port);
    clientfd = accept(linuxNet->sockfd, NULL, NULL);
    if(clientfd < 0){
        printf("{NET} Failed to accept client\n");
        goto err_close;
    }

    linuxNet->clientfd = clientfd;
    net.prv = (void *)linuxNet;

    ServerService(this);
err_close:
    delete linuxNet;
}

////////////////////////////////////////////////////////////
//                    C L I E N T                         //
////////////////////////////////////////////////////////////
void RPCClient::PushBinder(RPCBinder *binder){
    binders.push_back(binder);
}

void RPCClient::ConnectTo(const char *ip, int port){
    int sock = LinuxNetwork_CreateClientSocket(ip, port);
    if(sock < 0){
        printf("{NET} Failed to connect to %s:%d\n", ip, port);
        return;
    }

    printf("Connected\n");
    close(sock);
}

void RPCClient::WaitMessage(RPCMessage &message, int timeout){

}
