#ifndef NET_SESSION_H
#define NET_SESSION_H

#include "netutils.h"
#include "netuser.h"
#include "socket.h"
#include <stdlib.h>
#include <vector>
#include "../voxels/ChunksStorage.h"
#include "../coders/json.h"

// CURRENT TASK: fix problem with serializing chunk data 

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
    NetMode sesMode;
    std::vector<NetUser *> users;
    Socket socket;
    Level *sharedLevel;
    ConnectionData connData;
    NetPackage pkgToSend;
    bool serverUpdate;

    std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>> chunksQueue;

private:
    static NetSession *sessionInstance;

public:
    static NetSession *StartSession(NetMode type, Level *sl);
    static void TerminateSession();
    static NetSession *GetSessionInstance() { return sessionInstance; }
    static void NetUpdate(float delta) noexcept { if(sessionInstance) { sessionInstance->Update(delta); }}

public:
    server void HandleConnection(Player *rp, int ui);
    bool ConnectToSession(const char *ip, Engine *eng, bool versionChecking, bool contentChecking);
    bool StartServer();
    ConnectionData GetConnectionData() const { return connData; }
    void Update(float delta) noexcept;
    void RegisterMessage(const NetMessage& msg) noexcept;
    void SetSharedLevel(Level *sl);
    void ClientFetchChunk(std::shared_ptr<Chunk> chunk, int x, int z);

public:
    NetMode GetSessionType() const;
    NetUser *GetUser(size_t i) const; // user 0 is constantly the local user

private:
    NetSession(NetMode type, Player *lp, Level *sl);
    ~NetSession();

private:
    void ServerRoutine();
    void ClientRoutine();
    void ProcessPackage(NetPackage *pkg);
    ubyte *ServerGetChunk(int x, int z);
    NetUser *AddUser(NetUserRole role, Player *pl, int id);
};

#endif // NET_SESSION_H