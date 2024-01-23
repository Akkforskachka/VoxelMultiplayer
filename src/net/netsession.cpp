#include "netsession.h"

#include <iostream>
#include <stdlib.h>
#include <ctime>

#include "../world/Level.h"
#include "../world/World.h"
#include "../objects/Player.h"
#include "../voxels/Chunks.h"
#include "../content/Content.h"
#include "../voxels/Block.h"
#include "../engine.h"
#include "../voxels/Chunk.h"
#include "../voxels/Chunks.h"

namespace fs = std::filesystem;

NetSession *NetSession::sessionInstance = nullptr;

NetSession *NetSession::StartSession(NetMode type, Level *sl)
{
    if(sessionInstance != nullptr)
    {
        std::cout << "[WARNING]: Session is alreadty started!\n";
        TerminateSession();
    }
    if(sl)
        sessionInstance = new NetSession(type, sl->player, sl);
    else
        sessionInstance = new NetSession(type, nullptr, nullptr);
    return sessionInstance;
}

void NetSession::TerminateSession()
{
    if(sessionInstance == nullptr)
        return;

    delete sessionInstance;
    sessionInstance = nullptr;
}

NetSession::NetSession(NetMode mode, Player *lp, Level *sl) : 
    sesMode(mode), 
    sharedLevel(sl)
{
}

NetSession::~NetSession()
{
    std::cout << "[INFO]: Terminating session" << std::endl;
    for(NetUser *usr : users)
    {
        delete usr;
    }
    users.clear();
}

bool NetSession::StartServer()
{
    if(sesMode == NetMode::PLAY_SERVER) 
    {
        socket = Socket();
        if(socket.StartupServer(NET_PORT))
        {
            std::cout << "[INFO]: Creating NetSession. NetMode = " << sesMode << std::endl;
            AddUser(NetUserRole::AUTHORITY, sharedLevel->player, 0);
            std::cout << "[INFO]: Server started" << std::endl;
            return true;
        }
        else return false;
    }
    return false;
}


void NetSession::SetSharedLevel(Level *sl)
{
    sharedLevel = sl;
}

void NetSession::HandleConnection(Player *rp, socketfd ui)
{
    AddUser(NetUserRole::REMOTE, rp, ui);
    std::cout << "[INFO]: Handling new connection, uniq_id = " << ui << std::endl;

    sharedLevel->world->write(sharedLevel);

    size_t blCount = sharedLevel->content->indices->countBlockDefs();

    json::JObject worldData = json::JObject();
    worldData.put("seed", sharedLevel->world->seed);
    worldData.put("count", blCount);
    worldData.put("name", sharedLevel->world->name);
    worldData.put("user", ui);

    json::JObject& verObj = worldData.putObj("version");
    verObj.put("major", ENGINE_VERSION_MAJOR);
    verObj.put("minor", ENGINE_VERSION_MINOR);


    /* sharing the whole list of block names can make message too large for sending and recieving*/

    // json::JArray& cont = worldData.putArray("content");

    // for(size_t i = 0; i < blCount; i++)
    // {
    //     cont.put(sharedLevel->content->indices->getBlockDefs()[i]->name);
    // }

    std::string msg = json::stringify(&worldData, false, "");
    socket.SendMessage(msg.c_str(), msg.length(), ui, false);
}

bool NetSession::ConnectToSession(const char *ip, Engine *eng, bool versionChecking, bool contentChecking)
{
    socket = Socket();
    if(!socket.ConnectTo(ip, NET_PORT))
    {
        std::cout << "[ERROR]: Couldn't connect for some reason" << std::endl;
        return false;
    }
    std::cout << "[INFO]: Connected, waiting for initial message" << std::endl;
    char buff[128];
    socket.RecieveMessage(buff, 128, socket.sockfd, true);
    std::cout << "[INFO]: Connection message: " << buff << std::endl;

    json::JObject *data = json::parse(buff);

    connData.name = data->getStr("name", "");
    connData.seed = data->getInteger("seed", 0);
    connData.blockCount = data->getInteger("count", 0);
    connData.major = data->obj("version")->getInteger("major", 0);
    connData.minor = data->obj("version")->getInteger("minor", 0);
    connData.userID = data->getInteger("user", -1);

    // for(json::Value *val : data->arr("content")->values)
    // {
    //     if(val->type == json::valtype::string)
    //     {
    //         connData.blockNames.push_back(val->value.str->c_str());
    //     }
    // }

    if(versionChecking)
    {
        if(connData.major != ENGINE_VERSION_MAJOR || connData.minor != ENGINE_VERSION_MINOR)
            return false;
    }

    if(contentChecking)
    {
        if(connData.blockCount != eng->getContent()->indices->countBlockDefs())
            return false;

        // for(std::string name : connData.blockNames)
        // {
        //     if(engine->getContent()->findBlock(name) == nullptr)
        //     {
        //         return false;
        //     }
        // }
    }
    AddUser(NetUserRole::LOCAL, nullptr, connData.userID);
    return true;
}

NetUser *NetSession::AddUser(NetUserRole role, Player *pl, int id)
{
    NetUser *user = new NetUser(role, pl, id);
    std::cout << "[INFO]: Adding NetUser with userID  = "  << user->userID << std::endl;
    users.push_back(user);
    return user;
}

void NetSession::ProcessPackage(NetPackage *pkg)
{
    for(int i = 0; i < pkg->msgCount; i++)
    {
        NetMessage *msg = &pkg->messages[i]; 
        switch(msg->action)
        {
            case NetAction::SERVER_UPDATE:
                serverUpdate = true;
            break;
            case NetAction::MODIFY:
                sharedLevel->chunks->set((int)msg->coordinates.x, (int)msg->coordinates.y, (int)msg->coordinates.z, msg->block, msg->states);
                pkgToSend.msgCount -= 1; // some trash, need to fix it later
            break;
            // case NetAction::FETCH:
                // if(sesMode == NetMode::CLIENT)
                // {
                //     std::shared_ptr<Chunk> chunk = chunksQueue.at({msg->coordinates.x, msg->coordinates.z});
                //     std::cout << chunk->decode(msg->data) << std::endl;
                //     chunk->setLoaded(true);
                // }
                // else if(sesMode == NetMode::PLAY_SERVER)
                // {
                //     ubyte *data = ServerGetChunk((int)msg->coordinates.x, (int)msg->coordinates.z);
                //     NetMessage fm = NetMessage();
                //     fm.action = NetAction::FETCH;
                //     fm.coordinates.x = msg->coordinates.x;
                //     fm.coordinates.z = msg->coordinates.z;
                //     fm.data = data;
                //     RegisterMessage(fm);
                // }
            // break;
        }
    }
}

void NetSession::ServerRoutine()
{
    if(socket.UpdateServer(users))
    {
        while(socket.HasNewConnection())
        {
            socketfd cl = socket.GetNewConnection();
            HandleConnection(nullptr, cl);
        }

        NetPackage inPkg = NetPackage();
        while(socket.RecievePackage(&inPkg))
        {
            ProcessPackage(&inPkg);
        }

        NetPackage servPkg = NetPackage();
        // NetMessage upd = NetMessage();
        // upd.action = NetAction::SERVER_UPDATE;
        // servPkg.messages[0] = upd;
        // servPkg.msgCount = 1;
        for(int i = 0; i - 1 < pkgToSend.msgCount; i++)
        {
            servPkg.messages[i] = pkgToSend.messages[i - 1];
            servPkg.msgCount++;
        }

        if(servPkg.msgCount <= 0) return;
         
        for(NetUser *usr : users)
        {
            if(usr->isConnected)
                socket.SendPackage(&servPkg, usr->userID);
        }
        pkgToSend = NetPackage();
    }
}

void NetSession::ClientRoutine()
{
    if(socket.UpdateClient())
    {
        NetPackage inPkg = NetPackage();
        while(socket.RecievePackage(&inPkg))
        {
            ProcessPackage(&inPkg);
        }
    }

    if(serverUpdate)
    {
        if(pkgToSend.msgCount > 0)
        {
            socket.SendPackage(&pkgToSend, socket.sockfd);
            pkgToSend = NetPackage();
        }
        serverUpdate = false;
    }
}

void NetSession::ClientFetchChunk(std::shared_ptr<Chunk> chunk, uint& x, uint& z)
{
    // chunksQueue.insert_or_assign({x, z}, chunk);

    // NetMessage fm = NetMessage();
    // fm.action = NetAction::FETCH;
    // fm.coordinates.x = x;
    // fm.coordinates.z = z;
    // RegisterMessage(fm);
}

ubyte *NetSession::ServerGetChunk(int x, int z)
{
    std::shared_ptr<Chunk> chunk = sharedLevel->chunksStorage->get(x, z);

    if(chunk)
    {
        ubyte *buffer = new ubyte[CHUNK_DATA_LEN];
        std::cout << chunk.get()->decode(buffer) << std::endl;
        return buffer;
    }
    else
    {
        // TODO: generate the chunk
        return nullptr;
    }
}

float serv_delta = 0;
void NetSession::Update(float delta) noexcept
{
    serv_delta += delta;
    if(serv_delta >= 1 / SERVER_BIT_RATE)
    {
        serv_delta = 0;
        serverUpdate = true;
    }
    switch(sesMode)
    {
        case NetMode::PLAY_SERVER:
                ServerRoutine();
        break;
        case NetMode::CLIENT:
            ClientRoutine();
        break;
        case NetMode::STAND_ALONE:
        return;
    }
}

void NetSession::RegisterMessage(const NetMessage& msg) noexcept
{    
    if(pkgToSend.msgCount >= MAX_PACKAGE_SIZE - 1)
        return;

    pkgToSend.messages[pkgToSend.msgCount] = msg;
    pkgToSend.msgCount++;
}

NetMode NetSession::GetSessionType() const
{
    return sesMode;
}

NetUser *NetSession::GetUser(size_t i) const
{
    return users[i];
}


