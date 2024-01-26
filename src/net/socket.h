#ifndef SOCKET_H
#define SOCKET_H

#include "netutils.h"
#include <stdlib.h>
#include <vector>
#include "netpackage.h"

#include "../util/stringutil.h"

#ifdef _WIN32
    // TODO: windows API
#else
    #include <unistd.h>
    #include <stdio.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <fcntl.h>
#endif // _WIN32

class NetUser;

// Socket class defines socket for both client and server
class Socket
{
private:
#ifdef _WIN32
    // TODO: windows API
#else
    server_client struct sockaddr_in serv_addr;
    server struct sockaddr_in cli_addr;
    server std::vector<socketfd> connected;
    server size_t lastClient;
    server socketfd clientfd;
    server socklen_t clilen;
    server bool isRunning;
    server bool isServer;

    client struct hostent *serv;
    client bool isConnected;
#endif // _WIN32

    server_client std::vector<NetPackage> inPkg;

public:
    server_client socketfd sockfd;
public:
    server bool StartupServer(const int port);
    client bool ConnectTo(const char *ip, const int port);
    
    client void Disconnect();
    server_client void CloseSocket();

public:
    server_client bool SendPackage(NetPackage *pckg, socketfd sock);
    server_client bool RecievePackage(NetPackage *pckg);
    server bool UpdateServer(const std::vector<NetUser *> ins);
    client bool UpdateClient();

public:
    server bool HasNewConnection() const { return connected.size() > 0; }
    server socketfd GetNewConnection() { socketfd ret = connected[connected.size() - 1];
                                        connected.pop_back(); 
                                        return ret; }

    // buff -buffer to store data, length - length of message to read, sen - sender of a message, wait - should wait for a message?
    server_client bool RecieveMessage(char *buff, int length, socketfd sen, bool wait);
    // buff -buffer to store data, length - length of message to read, dest - destination, wait - should wait for a message?
    server_client bool SendMessage(const char *msg, int length, socketfd dest, bool wait);

private:
    void Deserialize(const char *buff);
};

#endif // SOCKET_H