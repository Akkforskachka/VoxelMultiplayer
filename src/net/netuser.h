#ifndef NET_USER_H
#define NET_USER_H

#include "netutils.h"

class Player;
class PlayerController;

class server_client NetUser 
{

private:
    // Player *localPlayer;
    // PlayerController *localController;
    NetUserRole role;

    std::string userName;
    uniqueUserID userID;
    
public:
    bool isApproved;
    bool isConnected;

public:
    NetUser(NetUserRole r, int ui);
    ~NetUser();

public:
    // Player *GetLocalPlayer() const;
    NetUserRole GetNetRole() const;
    uniqueUserID GetUniqueUserID() const;
    std::string GetUserName() const;
};

#endif