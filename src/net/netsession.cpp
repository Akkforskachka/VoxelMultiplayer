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
#include "../voxels/ChunksStorage.h"
#include "../lighting/Lighting.h"

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

    size_t blCount = sharedLevel->content->getIndices()->countBlockDefs();

    dynamic::Map worldData = dynamic::Map();
    worldData.put("seed", sharedLevel->world->getSeed());
    worldData.put("count", blCount);
    worldData.put("name", sharedLevel->world->getName());
    worldData.put("user", ui);

    dynamic::Map& verObj = worldData.putMap("version");
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
    char buff[NetSize()];
    socket.RecieveMessage(buff, NetSize(), socket.sockfd, true);
    std::cout << "[INFO]: Connection message: " << buff << std::endl;

    std::unique_ptr<dynamic::Map> data = json::parse(buff);

    data->num("seed", connData.seed);
    data->num("count", connData.blockCount);
    data->map("version")->num("major", connData.major);
    data->map("version")->num("minor", connData.minor);
    data->num("user", connData.userID );

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
        if(connData.blockCount != eng->getContent()->getIndices()->countBlockDefs())
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
    bool b = false;
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
            case NetAction::FETCH:
                if(sesMode == NetMode::CLIENT)
                {
                    std::shared_ptr<Chunk> chunk = sharedLevel->chunksStorage->get((int)msg->coordinates.x, (int)msg->coordinates.z);
                    if(chunk == nullptr) continue;
                    if(chunk->isLoaded()) continue;;
                    if(msg->data)
                    {
                        chunk->decode(msg->data);

                        // chunksQueue.erase({msg->coordinates.x, msg->coordinates.z});

                        sharedLevel->chunks->putChunk(chunk);

                        for (size_t i = 0; i < CHUNK_VOL; i++) {
                            blockid_t id = chunk->voxels[i].id;
                            if (sharedLevel->content->getIndices()->getBlockDef(id) == nullptr) {
                                std::cout << "corruped block detected at " << i << " of chunk ";
                                std::cout << chunk->x << "x" << chunk->z;
                                std::cout << " -> " << (int)id << std::endl;
                                chunk->voxels[i].id = 11;
                            }
                        }

                        chunk->updateHeights();

                        Lighting::prebuildSkyLight(chunk.get(), sharedLevel->content->getIndices());

                        chunk->setLoaded(true);
                        chunk->setReady(false);
                        chunk->setLighted(false);
                    }                            
                }
                else if(sesMode == NetMode::PLAY_SERVER)
                {
                    if(b) continue;

                    ubyte *data = ServerGetChunk((int)msg->coordinates.x, (int)msg->coordinates.z);

                    if(data)
                    {
                        NetMessage fm = NetMessage();
                        fm.action = NetAction::FETCH;
                        fm.coordinates.x = msg->coordinates.x;
                        fm.coordinates.z = msg->coordinates.z;
                        fm.data = data;
                        b = true;
                        RegisterMessage(fm);
                    }
                }
            break;
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
        for(int i = 0; i < pkgToSend.msgCount; i++)
        {
            servPkg.messages[i] = pkgToSend.messages[i];
            servPkg.msgCount++;
        }
        NetMessage upd = NetMessage();
        upd.action = NetAction::SERVER_UPDATE;
        servPkg.messages[servPkg.msgCount++] = upd;
         
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
        auto it = chunksQueue.begin();
        if(it != chunksQueue.end())
        {
            NetMessage fm = NetMessage();
            fm.action = NetAction::FETCH;
            fm.coordinates.x = it->second->x;
            fm.coordinates.z = it->second->z;
            RegisterMessage(fm);
            chunksQueue.erase(it->first);
        }

        if(pkgToSend.msgCount > 0)
        {
            socket.SendPackage(&pkgToSend, socket.sockfd);
            pkgToSend = NetPackage();
        }
        serverUpdate = false;
    }
}

void NetSession::ClientFetchChunk(std::shared_ptr<Chunk> chunk, int x, int z)
{
    chunksQueue.insert_or_assign({x, z}, chunk);
}

ubyte *NetSession::ServerGetChunk(int x, int z)
{
    std::shared_ptr<Chunk> chunk = sharedLevel->chunksStorage->get(x, z);

    if(chunk)
    {
        if(chunk->isReady())
        {
            ubyte *buffer = chunk->encode();
            return buffer;
        }
    }
    // TODO: generate chunk
    return nullptr;
}

float serv_delta = 0;
void NetSession::Update(float delta) noexcept
{
    serv_delta += delta;
    switch(sesMode)
    {
        case NetMode::PLAY_SERVER:
            if(serv_delta >= 1 / SERVER_BIT_RATE)
            {
                ServerRoutine();
                serv_delta = 0;
                serverUpdate = true;
            }
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


