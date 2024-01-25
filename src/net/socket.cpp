#include "socket.h"
#include <strings.h>
#include <string.h>
#include <iostream>
#include "../coders/json.h"
#include "netuser.h"
#include "../coders/gzip.h"
#include "../voxels/Chunk.h"

bool Socket::StartupServer(const int port)
{
#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((char *) &serv_addr, sizeof(serv_addr));
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
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    return true;
#endif // _WIN32
}

bool Socket::ConnectTo(const char *ip, int port)
{
    isConnected = false;
    isServer = false;
#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else
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
    bzero((char *)&serv_addr, sizeof(serv_addr));
    bcopy((char *)serv->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        serv->h_length);

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
#endif // _WIN32
}

bool Socket::UpdateServer(const std::vector<NetUser *> ins)
{
    if(isRunning == false) return false; 

#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else

    cli_addr = {0};
    socketfd clifd = accept(sockfd, 
                    (struct sockaddr *) &cli_addr, 
                    &clilen);
    if(clifd > 0)
    {
        connected.push_back(clifd);
    }

    for(NetUser *usr : ins)
    {
        char buff[NetSize()] = "";

        if(usr->isConnected == false)
            continue;
            
        if(RecieveMessage(buff, NetSize(), usr->userID, false))
        {
            Deserialize(buff);
        }
        else
        {
            if(errno == 104)
            {
                std::cout << "Client just disconnected\n";
                usr->isConnected = false;
            }
        }
    }

    return true;
#endif // _WIN32
    return false;
}

bool Socket::UpdateClient()
{
    if(isConnected == false) return false; 
#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else
    char buff[NetSize()] = "";
    if(RecieveMessage(buff, NetSize(), sockfd, false))
    {
        Deserialize(buff);
        return true;
    }
#endif // _WIN32
    return false;
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
        std::cerr << e.what() << " in message: " << buff << std::endl;
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
            stmsg.data = nullptr;
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
                    std::vector<ubyte> decompressed = gzip::decompress((ubyte *)messg->getStr("data", "").data(), 
                                                                    messg->getStr("data", "").length());
                    stmsg.data = decompressed.data();
                }
                sPkg.messages[sPkg.msgCount++] = stmsg;
                stmsg = NetMessage();
            }
        }
        inPkg.push_back(sPkg);
    }
}


void Socket::Disconnect()
{
    CloseSocket();
}

void Socket::CloseSocket()
{
#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else
    close(sockfd);
#endif // _WIN32
}

bool Socket::RecieveMessage(char *buff, int length, socketfd sen, bool wait)
{
    if((isRunning || isConnected) == false) return false;
#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else
    int r = recv(sen, buff, length, wait ? 0 : MSG_DONTWAIT);
    // TODO: implement error checking
    return r > 0;
#endif // _WIN32
}

bool Socket::SendMessage(const char *msg, int length, socketfd dest, bool wait)
{
    if((isRunning || isConnected) == false) return false;

#ifdef _WIN32
    assert(0 && "TODO: Implement windows API");
#else 
    int r = send(dest, msg, length, wait ? 0 : MSG_DONTWAIT);
    if(r > 0)
        std::cout << "sent message of size " << length << std::endl;
    return r > 0;
#endif // _WIN32
}

bool Socket::SendPackage(NetPackage *pckg, socketfd sock)
{
    // Serialize package and send it
    
    dynamic::Map spk = dynamic::Map();
    dynamic::List& smgs = spk.putList("messages");

    for(int i = 0; i < pckg->msgCount; i++)
    {
        dynamic::Map& msg = smgs.putMap();
        msg.put("usr_id", pckg->messages[i].usr_id);
        msg.put("action", pckg->messages[i].action);
        dynamic::List& p = msg.putList("coordinates");
        switch(pckg->messages[i].action)
        {
            case NetAction::MODIFY:
                msg.put("block", pckg->messages[i].block);
                p.put(pckg->messages[i].coordinates.x);
                p.put(pckg->messages[i].coordinates.y);
                p.put(pckg->messages[i].coordinates.z);
                msg.put("states", pckg->messages[i].states);
            break;
            case NetAction::FETCH:
                p.put(pckg->messages[i].coordinates.x);
                p.put(pckg->messages[i].coordinates.y);
                p.put(pckg->messages[i].coordinates.z);
                if(pckg->messages[i].data)
                {
                    std::vector<ubyte> compressed = gzip::compress(pckg->messages[i].data, CHUNK_DATA_LEN);
                    msg.put("data", std::string((const char *)compressed.data(), compressed.size()));
                }
            break;
        }
    }
    spk.put("count", pckg->msgCount);

    std::string buff = json::stringify(&spk, false, "");
    return SendMessage(buff.c_str(), buff.length(), sock, false);
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
