#ifndef NET_USER_H
#define NET_USER_H

#include "netutils.h"

class Player;
class PlayerController;

class server_client NetUser 
{

private:
    Player *localPlayer;
    // PlayerController *localController;
    NetUserRole role;
    
public:
    int userID;
    bool isApproved;
    bool isConnected;

public:
    NetUser(NetUserRole r, Player *lp, int ui);
    ~NetUser();

public:
    Player *GetLocalPlayer() const;
    NetUserRole GetNetRole() const;
};

#endif