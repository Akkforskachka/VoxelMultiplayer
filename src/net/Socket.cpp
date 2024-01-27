#include "Socket.h"
#include <string.h>
#include <iostream>
#include <iterator>
#include <numeric>
#include "../coders/json.h"
#include "NetUser.h"
#include "../coders/gzip.h"
#include "../voxels/Chunk.h"

#ifdef _WIN32
static void setSocketIOMode(SOCKET socket, long cmd, bool wait) {
    u_long iMode = !wait;
    int iResult = ioctlsocket(socket, cmd, &iMode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);
};
WSADATA data{};
#else
#include <strings.h>
#endif // _WIN32

bool Socket::StartupServer(const int port)
{
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &data);
#endif // _WIN32
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    ZeroMemory(&serv_addr, sizeof(serv_addr));
    setSocketIOMode(sockfd, FIONBIO, false);

    char name[64];
    gethostname(name, 64);
    struct hostent* ent = gethostbyname(name);
    struct in_addr ip_addr = *(struct in_addr*)(ent->h_addr);
    printf("Hostname: %s, was resolved to: %s\n",
        name, inet_ntoa(ip_addr));
#else
    bzero((char *) &serv_addr, sizeof(serv_addr));
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif // _WIN32

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    int r = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (r < 0)
    {
        fprintf(stderr, "[ERROR]: Couldn't bind a server %d\n", r);
        CloseSocket();
        return false;
    }
    listen(sockfd, MAX_CONN);
    isRunning = true;
    isServer = true;
    clilen = sizeof(cli_addr);

    return true;
}

bool Socket::ConnectTo(const char *ip, int port)
{
    isConnected = false;
    isServer = false;

#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 2), &data);
#endif // _WIN32
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        fprintf(stderr, "[ERROR]: Couldn't open a client socket\n");
        return false;
    }

    serv = gethostbyname(ip);
    if(serv == nullptr)
    {
        fprintf(stderr, "[ERROR}: Can't find exact server %s\n", ip);
        return false;
    }
#ifdef _WIN32
    ZeroMemory(&serv_addr, sizeof(serv_addr));
    CopyMemory(&serv->h_addr, &serv_addr.sin_addr.s_addr, serv->h_length);
    serv_addr.sin_addr.s_addr = inet_addr(ip);
#else
    bzero((char *)&serv_addr, sizeof(serv_addr));
    bcopy((char *)serv->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        serv->h_length);
#endif // _WIN32

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    int ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(ret < 0)
    {
        CloseSocket();
        return false;
    }
    else
    {
        isConnected = true;
        return true;
    }
    return true;
}

bool Socket::UpdateServer(const std::unordered_map<uniqueUserID, NetUser *> ins)
{
    if(isRunning == false) return false; 

    cli_addr = {0};
    socketfd clifd = accept(sockfd, 
                    (struct sockaddr *) &cli_addr, 
                    &clilen);
#ifdef _WIN32
    if (clifd != INVALID_SOCKET)
#else
    if(clifd > 0)
#endif // _WIN32
    {
        connected.push_back(clifd);
    }

    for(auto pair : ins)
    {
        NetUser *usr = pair.second;
        if(usr->GetUniqueUserID() == 0) continue;;
        
        char buff[NetSize()] = "";

        if(usr->isConnected == false)
            continue;
        
        int bytes = RecieveMessage(buff, NetSize(), usr->GetUniqueUserID(), false);
        if(bytes > 0)
        {
            Deserialize(buff);
        }
    }

    return true;
}

bool Socket::UpdateClient()
{
    if(isConnected == false) return false; 

    char buff[NetSize()] = "";
    int bytes = RecieveMessage(buff, NetSize(), sockfd, false);
    if(bytes > 0)
    {
        Deserialize(buff);
        return true;
    }

    return false;
}

void Socket::Disconnect()
{
    CloseSocket();
}

void Socket::CloseSocket()
{
#ifdef _WIN32
    WSACleanup();
    closesocket(sockfd);
#else
    close(sockfd);
#endif // _WIN32
}

int Socket::RecieveMessage(char *buff, int length, socketfd sen, bool wait)
{
    if((isRunning || isConnected) == false) return false;
#ifdef _WIN32
    int r = recv(sen, buff, length, 0);
#else
    int r = recv(sen, buff, length, wait ? 0 : MSG_DONTWAIT);
#endif // _WIN32
    return r;
}

int Socket::SendMessage(const char *msg, int length, socketfd dest, bool wait)
{
    if((isRunning || isConnected) == false) return false;

#ifdef _WIN32
    int r = send(dest, msg, length, 0);
#else 
    int r = send(dest, msg, length, wait ? 0 : MSG_DONTWAIT);
#endif // _WIN32
    if(r <= 0)
    {
        std::cout << "PACKAGE LOsT" << std::endl;
    }
    return r;
}

void Socket::Deserialize(const char *buff)
{
    std::unique_ptr<dynamic::Map> pckg;
    try
    {
        pckg = json::parse(buff);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << " in message " << buff << std::endl;
        return;
    }        

    if(pckg)
    {
        NetPackage sPkg = NetPackage();
        int mCount = 0;
        if(pckg->has("count"))
        {
            pckg->num("count", mCount);
        }
        if(pckg->has("messages"))
        {
            dynamic::List *msgs = pckg->list("messages");
            NetMessage stmsg = NetMessage();
            for(int i = 0; i < mCount; i++)
            {
                dynamic::Map *messg = msgs->map(i); 
                stmsg.usr_id = 0;
                if(messg->has("action"))
                {
                    int act;
                    messg->num("action", act);
                    stmsg.action = (NetAction) act;
                }
                if(messg->has("block"))
                {
                    messg->num("block", stmsg.block);
                }                
                if(messg->has("states"))
                {
                    messg->num("states", stmsg.states);
                }
                if(messg->has("coordinates"))
                {
                    dynamic::List *cord = messg->list("coordinates");
                    if(cord->size() == 3)
                    {
                        stmsg.coordinates.x = cord->num(0);
                        stmsg.coordinates.y = cord->num(1);
                        stmsg.coordinates.z = cord->num(2);
                    }
                }
                if(messg->has("data"))
                {
                    std::vector<ubyte> data = util::base64_decode(messg->getStr("data", ""));
                    std::vector<ubyte> decompressed = gzip::decompress(data.data(), data.size() - 1);

                    stmsg.data = new ubyte[CHUNK_DATA_LEN];
                    memcpy(stmsg.data, decompressed.data(), CHUNK_DATA_LEN);
                }
                sPkg.AddMessage(stmsg);
            }
        }
        inPkg.push_back(sPkg);
    }
}

bool Socket::SendPackage(NetPackage *pckg, socketfd sock)
{
    // Serialize package and send it

    dynamic::Map spk = dynamic::Map();
    dynamic::List& smgs = spk.putList("messages");

    for(size_t i = 0; i < pckg->GetMessagesCount(); i++)
    {
        dynamic::Map& msg = smgs.putMap();
        msg.put("usr_id", pckg->GetMessage(i).usr_id);
        msg.put("action", pckg->GetMessage(i).action);
        dynamic::List& p = msg.putList("coordinates");
        switch(pckg->GetMessage(i).action)
        {
            case UNKOWN:
            break;
            case NetAction::SERVER_UPDATE:
                p.put(pckg->GetMessage(i).coordinates.x);
                p.put(pckg->GetMessage(i).coordinates.y);
                p.put(pckg->GetMessage(i).coordinates.z);
            break;
            case NetAction::MODIFY:
                msg.put("block", pckg->GetMessage(i).block);
                p.put(pckg->GetMessage(i).coordinates.x);
                p.put(pckg->GetMessage(i).coordinates.y);
                p.put(pckg->GetMessage(i).coordinates.z);
                msg.put("states", pckg->GetMessage(i).states);
            break;
            case NetAction::FETCH:
                p.put(pckg->GetMessage(i).coordinates.x);
                p.put(pckg->GetMessage(i).coordinates.y);
                p.put(pckg->GetMessage(i).coordinates.z);
                if(pckg->GetMessage(i).data)
                {
                    std::vector<ubyte> compressed = gzip::compress(pckg->GetMessage(i).data, CHUNK_DATA_LEN);
                    std::string text = util::base64_encode(compressed.data(), compressed.size());
                    msg.put("data", text);
                }
            break;
        }
    }
    spk.put("count", pckg->GetMessagesCount());

    std::string buff = json::stringify(&spk, false, "");
    return SendMessage(buff.c_str(), buff.length(), sock, false) > 0;
}

bool Socket::RecievePackage(NetPackage *pckg)
{
    if(inPkg.size() > 0)
    {
        *pckg = inPkg[inPkg.size() - 1];
        inPkg.pop_back();
        return true;
    }
    return false;
}
