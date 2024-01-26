#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <unordered_map>
#include <glm/glm.hpp>
#include "../typedefs.h"
#include <vector>
#include <string>

#define server // code/data runs/exists only on a server
#define client // code/data runs/exists only on a client
#define server_client // code/data runs/exists both on server and clients

#define NET_PORT 6969
#define SERVER_BIT_RATE 30
#define MAX_CONN 5
#define MAX_MESSAGES_PER_PACKET 20

#ifdef _WIN32
#else
    typedef int socketfd;
#endif // _WIN32

typedef int uniqueUserID;

enum NetMode
{
    STAND_ALONE,
    PLAY_SERVER,
    CLIENT
};

enum NetUserRole
{
    AUTHORITY, // server
    LOCAL, // local owner
    REMOTE // remote client
};

enum NetAction
{
    UNKOWN,
    SERVER_UPDATE,
    FETCH,
    MODIFY,
};

struct NetMessage
{
    int usr_id;
    NetAction action;
    struct Coordinates 
    {
        float x;
        float y;
        float z;
    } coordinates;
    int block;
    uint8_t states;
    ubyte *data = nullptr;
};

constexpr int NetSize()  { return 5128; } // todo


#endif // NET_UTILS_H