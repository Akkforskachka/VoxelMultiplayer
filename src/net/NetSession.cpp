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
#include "../frontend/WorldRenderer.h"

namespace fs = std::filesystem;

NetSession *NetSession::sessionInstance = nullptr;
ConnectionData *NetSession::connData = nullptr;

void NetSession::TerminateSession()
{
    if(sessionInstance == nullptr)
        return;
    delete sessionInstance;
}

NetSession *NetSession::GetSessionInstance()
{
    return sessionInstance;
}

NetSession::NetSession(NetMode mode) : netMode(mode)
{
    messagesBuffer = std::vector<NetMessage>();
    serverUpdate = false;
}

NetSession::~NetSession()
{
    std::cout << "[INFO]: Terminating session" << std::endl;
    for(NetUser *usr : users)
    {
        delete usr;
    }
    socket.CloseSocket();
    users.clear();
}

bool NetSession::StartSession(Engine *engine, const int port)
{
    if(sessionInstance)
    {
        return false;
    }

    sessionInstance = new NetSession(NetMode::PLAY_SERVER);

    sessionInstance->socket = Socket();
    if(sessionInstance->socket.StartupServer(port))
    {
        std::cout << "[INFO]: Creating NetSession. NetMode = " << sessionInstance->netMode << std::endl;
        sessionInstance->addUser(NetUserRole::AUTHORITY, 0);
        std::cout << "[INFO]: Server started" << std::endl;
        return true;
    }
    return false;
}

bool NetSession::ConnectToSession(const char *ip, const int port, Engine *eng, bool versionChecking, bool contentChecking)
{
    if(sessionInstance) 
    {
        return false;
    }
    sessionInstance = new NetSession(NetMode::CLIENT);
    std::cout << "Connecting to " << ip << std::endl;
    sessionInstance->socket = Socket();
    if(!sessionInstance->socket.ConnectTo(ip, port))
    {
        std::cout << "[ERROR]: Couldn't connect. Error code: " << errno << std::endl;
        return false;
    }
    std::cout << "[INFO]: Connected, waiting for initial message" << std::endl;
    char buff[NetSize()];
    if(sessionInstance->socket.RecieveMessage(buff, NetSize(), sessionInstance->socket.sockfd, true) > 0)
    {
        std::cout << "[INFO]: Connection message: " << buff << std::endl;

        std::unique_ptr<dynamic::Map> data = json::parse(buff);
        connData = new ConnectionData();

        data->num("seed", connData->seed);
        data->num("count", connData->blockCount);
        data->map("version")->num("major", connData->major);
        data->map("version")->num("minor", connData->minor);
        data->num("user", connData->userID );
        connData->name = data->getStr("name", "err");

        // for(json::Value *val : data->arr("content")->values)
        // {
        //     if(val->type == json::valtype::string)
        //     {
        //         connData.blockNames.push_back(val->value.str->c_str());
        //     }
        // }

        if(versionChecking)
        {
            if(connData->major != ENGINE_VERSION_MAJOR || connData->minor != ENGINE_VERSION_MINOR)
                return false;
        }

        if(contentChecking)
        {
            if(connData->blockCount != eng->getContent()->getIndices()->countBlockDefs())
                return false;

            // for(std::string name : connData.blockNames)
            // {
            //     if(engine->getContent()->findBlock(name) == nullptr)
            //     {
            //         return false;
            //     }
            // }
        }
        sessionInstance->addUser(NetUserRole::LOCAL, connData->userID);
        return true;
    }
    std::cout << "[ERROR]: Couldn't get connection message. Error code: " << errno << std::endl;
    TerminateSession();
    return false;
}
void NetSession::handleConnection(socketfd ui)
{
    addUser(NetUserRole::REMOTE, ui);
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

NetMode NetSession::GetSessionType()
{
    if(!sessionInstance)
    {
        return NetMode::STAND_ALONE;
    }
    return sessionInstance->netMode;
}


void NetSession::SetSharedLevel(Level *sl) noexcept
{
    if(sessionInstance)
    {
        sessionInstance->sharedLevel = sl;
        std::cout << "shared level been set\n";
    }
}

NetUser *NetSession::addUser(NetUserRole role, int id)
{
    NetUser *user = new NetUser(role, id);
    user->isConnected = true;
    std::cout << "[INFO]: Adding NetUser with userID  = "  << user->GetUniqueUserID() << std::endl;
    users.push_back(user);
    return user;
}

void NetSession::packMessages(NetPackage *dst)
{
    for(size_t i = 0; dst->GetMessagesCount() < MAX_MESSAGES_PER_PACKET  && !messagesBuffer.empty(); ++i)
    {
        dst->AddMessage(messagesBuffer[i]);
        messagesBuffer.pop_back();
    }
}

void NetSession::processPackage(NetPackage *pkg)
{
    bool b = false;

    for(int i = 0; i < (int)pkg->GetMessagesCount(); i++)
    {
        NetMessage msg = pkg->GetMessage(i);
        switch(msg.action)
        {
            case UNKOWN:
            break;
            case NetAction::SERVER_UPDATE:
                serverUpdate = true;
                sharedLevel->getWorld()->daytime = msg.coordinates.x;
                WorldRenderer::fog = msg.coordinates.y;
            break;
            case NetAction::MODIFY:
                sharedLevel->chunks->set((int)msg.coordinates.x, (int)msg.coordinates.y, (int)msg.coordinates.z, msg.block, msg.states);
                messagesBuffer.pop_back(); // some shit again
            break;
            case NetAction::FETCH:
                if(netMode == NetMode::CLIENT)
                {
                    std::shared_ptr<Chunk> chunk = sharedLevel->chunksStorage->get((int)msg.coordinates.x, (int)msg.coordinates.z);
                    if(chunk == nullptr) continue;
                    if(chunk->isLoaded()) continue;
                    if(msg.data)
                    {
                        chunk->decode(msg.data);

                        for (size_t i = 0; i < CHUNK_VOL; i++) {
                            blockid_t id = chunk->voxels[i].id;
                            if (sharedLevel->content->getIndices()->getBlockDef(id) == nullptr) {
                                std::cout << "corruped block detected at " << i << " of chunk ";
                                std::cout << chunk->x << "x" << chunk->z;
                                std::cout << " -> " << (int)id << std::endl;
                                chunk->voxels[i].id = 11;
                            }
                        }

                        sharedLevel->chunks->putChunk(chunk);

                        chunk->updateHeights();
                        Lighting::prebuildSkyLight(chunk.get(), sharedLevel->content->getIndices());
                        chunk->setLoaded(true);
                        chunk->setReady(false);
                        chunk->setLighted(false);
                    }                            
                }
                else if(netMode == NetMode::PLAY_SERVER)
                {
                    if(b) continue;

                    ubyte *chunkData = serverGetChunk((int)msg.coordinates.x, (int)msg.coordinates.z);

                    if(chunkData)
                    {
                        b = true;
                        NetMessage fm = NetMessage();
                        fm.action = NetAction::FETCH;
                        fm.coordinates.x = msg.coordinates.x;
                        fm.coordinates.z = msg.coordinates.z;
                        fm.data = chunkData;
                        RegisterMessage(fm);
                    }
                }
            break;
        }
    }
}

void NetSession::serverRoutine()
{
    if(socket.UpdateServer(users))
    {
        while(socket.HasNewConnection())
        {
            socketfd cl = socket.GetNewConnection();
            handleConnection(cl);
        }

        NetPackage inPkg = NetPackage();
        while(socket.RecievePackage(&inPkg))
        {
            processPackage(&inPkg);
        }

        NetPackage servPkg = NetPackage();
        NetMessage upd = NetMessage();
        upd.action = NetAction::SERVER_UPDATE;
        if(sharedLevel)
        {
            upd.coordinates.x = sharedLevel->getWorld()->daytime;
            upd.coordinates.y = WorldRenderer::fog;
        }
        servPkg.AddMessage(upd);

        packMessages(&servPkg);

        for(size_t i = 0; servPkg.GetMessagesCount() < MAX_MESSAGES_PER_PACKET && !messagesBuffer.empty(); ++i)
        {
            servPkg.AddMessage(messagesBuffer[messagesBuffer.size() - i - 1]);
            messagesBuffer.pop_back();
        }
         
        for(NetUser *usr : users)
        {
            if(usr->isConnected && usr->GetUniqueUserID() != 0)
            {
                socket.SendPackage(&servPkg, usr->GetUniqueUserID());
            }
        }
    }
}

void NetSession::clientRoutine()
{
    if(socket.UpdateClient())
    {
        NetPackage inPkg = NetPackage();
        while(socket.RecievePackage(&inPkg))
        {
            processPackage(&inPkg);
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

        NetPackage pkgToSend = NetPackage();

        packMessages(&pkgToSend);

        if(pkgToSend.GetMessagesCount() > 0)
        {
            socket.SendPackage(&pkgToSend, socket.sockfd);
        }
        serverUpdate = false;
    }
}

void NetSession::ClientFetchChunk(std::shared_ptr<Chunk> chunk, int x, int z)
{
    chunksQueue.insert_or_assign({x, z}, chunk);
}

ubyte *NetSession::serverGetChunk(int x, int z) const
{
    std::shared_ptr<Chunk> chunk = sharedLevel->chunksStorage->get(x, z);

    // get chunk from memory
    if(chunk)
    {
        if(chunk->isLoaded())
        {
            return chunk->encode();
        }
    }

    // load and get chunk from hard drive
    // chunk = sharedLevel->chunksStorage->create(x, z);    
    // if(chunk->isLoaded())
    // {
    //     return chunk->encode();
    // }

    // generate chunk
    
    return nullptr;
}

float serv_delta = 0;
void NetSession::Update(float delta) noexcept
{
    if(!sharedLevel) return;

    serv_delta += delta;
    switch(netMode)
    {
        case NetMode::PLAY_SERVER:
            if(serv_delta >= 1 / SERVER_BIT_RATE)
            {
                serverRoutine();
                serv_delta = 0;
            }
        break;
        case NetMode::CLIENT:
            clientRoutine();
        break;
        case NetMode::STAND_ALONE:
        return;
    }
}

void NetSession::RegisterMessage(const NetMessage msg) noexcept
{
    messagesBuffer.push_back(msg);
}

NetUser *NetSession::GetUser(size_t i)
{
    if(sessionInstance)
    {
        return sessionInstance->users[i];
    }
    return nullptr;
}


