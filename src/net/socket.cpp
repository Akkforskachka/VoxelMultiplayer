#include "socket.h"
#include <strings.h>
#include <string.h>
#include <iostream>
#include "../coders/json.h"
#include "netuser.h"

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
    json::JObject *pckg;

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
            mCount = pckg->getInteger("count", 0);
        }
        if(pckg->has("messages"))
        {
            json::JArray *msgs = pckg->arr("messages");
            NetMessage stmsg = NetMessage();
            for(int i = 0; i < mCount; i++)
            {
                json::JObject *messg = msgs->obj(i); 
                stmsg.usr_id = 0;
                if(messg->has("action"))
                {
                    stmsg.action = (NetAction) messg->getInteger("action", 0);
                }
                if(messg->has("block"))
                {
                    stmsg.block = messg->getInteger("block", -1);
                }
                if(messg->has("coordinates"))
                {
                    json::JArray *cord = messg->arr("coordinates");
                    if(cord->size() == 3)
                    {
                        stmsg.coordinates.x = cord->integer(0);
                        stmsg.coordinates.y = cord->integer(1);
                        stmsg.coordinates.z = cord->integer(2);
                    }
                }
                if(messg->has("data"))
                {
                    stmsg.data = (ubyte *) messg->getStr("data", "").c_str();
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
    return r > 0;
#endif // _WIN32
}

bool Socket::SendPackage(NetPackage *pckg, socketfd sock)
{
    // Serialize package and send it
    
    json::JObject spk = json::JObject();
    json::JArray& smgs = spk.putArray("messages");

    for(int i = 0; i < pckg->msgCount; i++)
    {
        json::JObject& msg = smgs.putObj();
        msg.put("usr_id", pckg->messages[i].usr_id);
        msg.put("action", pckg->messages[i].action);
        json::JArray& p = msg.putArray("coordinates");
        switch(pckg->messages[i].action)
        {
            case NetAction::MODIFY:
                msg.put("block", pckg->messages[i].block);
                p.put(pckg->messages[i].coordinates.x);
                p.put(pckg->messages[i].coordinates.y);
                p.put(pckg->messages[i].coordinates.z);
                msg.put("states", pckg->messages[i].states);
            break;
            // case NetAction::FETCH:
            //     p.put(pckg->messages[i].coordinates.x);
            //     p.put(pckg->messages[i].coordinates.y);
            //     p.put(pckg->messages[i].coordinates.z);
            //     if(pckg->messages[i].data)
            //         msg.put("data", pckg->messages[i].data);
            // break;
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
