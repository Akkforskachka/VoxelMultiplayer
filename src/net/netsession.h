#ifndef NET_SESSION_H
#define NET_SESSION_H

#include "netutils.h"
#include "netuser.h"
#include "socket.h"
#include "netpackage.h"
#include <stdlib.h>
#include <vector>
#include "../voxels/ChunksStorage.h"
#include "../coders/json.h"



// CURRENT TASK: error handling

// TODO: better RPC calls - NetSession::RegisterMessage(object, event, replicationType, ...params)
// where replicationType defines wether RPC should replicate only on client, 
// on server and owning client, or across the whole network
// prpbably every object must have it's own replicated across the network ID, 
// that allows us to distinguish for what object we should call RPC
// we definitely need ability to make any RPC calls, without changing Net-code

// TODO: objects replication (variables replication)

// TODO: spawn remote players objects

// TODO: better serialization/deserialization

// TODO: fix client crash on loosing connection

// TODO: share player's nickname, tab menu, chat, player's movement ---- all of that needs better(unified) RPC calls

// TODO: save remote player's last location and restore on connection 

#define NET_MODIFY(id, states, c_x, c_y, c_z) 	do {                        \
                                            NetMessage msg = NetMessage();  \
                                            msg.action = NetAction::MODIFY; \
                                            msg.block = id;         \
                                            msg.states = states;    \
                                            msg.coordinates.x = c_x;  \
                                            msg.coordinates.y = c_y;  \
                                            msg.coordinates.z = c_z;  \
                                            if(NetSession *ses = NetSession::GetSessionInstance())  \
                                            {                                                       \
                                                msg.usr_id = ses->GetUser(0)->GetUniqueUserID();         \
                                                ses->RegisterMessage(msg);                          \
                                            }                                                       \
                                        } while(0)
class Player;
class Level;
class Engine;
class Chunk;

struct ConnectionData
{
    uint64_t seed;
    size_t blockCount;
    int userID;
    std::vector<std::string> blockNames;
    std::string name;
    int major;
    int minor;
};

class server_client NetSession
{
private:
    NetMode netMode;
    std::vector<NetUser *> users;
    Socket socket;
    Level *sharedLevel;
    // NetPackage pkgToSend;

    std::vector<NetMessage> messagesBuffer;

    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunksQueue;

    bool serverUpdate;

private:
    static ConnectionData *connData;
    static NetSession *sessionInstance;

public:
    static NetSession *GetSessionInstance();
    static bool StartServer(Engine *eng);
    static bool ConnectToSession(const char *ip, Engine *eng, bool versionChecking, bool contentChecking);
    static void TerminateSession();
    static NetUser *GetUser(size_t i); // user 0 is constantly the local user
    static const ConnectionData *GetConnectionData() { return connData; }
    static void SetSharedLevel(Level *sl) noexcept;
    static NetMode GetSessionType();

public:
    void Update(float delta) noexcept;
    void RegisterMessage(const NetMessage msg) noexcept;
    void ClientFetchChunk(std::shared_ptr<Chunk> chunk, int x, int z);

public:

private:
    NetSession(NetMode type);
    ~NetSession();

private:
    server void handleConnection(int ui);
    server void serverRoutine();
    client void clientRoutine();
    server_client void processPackage(NetPackage *pkg);
    server ubyte *serverGetChunk(int x, int z) const;
    server_client NetUser *addUser(NetUserRole role, int id);
};

#endif // NET_SESSION_H