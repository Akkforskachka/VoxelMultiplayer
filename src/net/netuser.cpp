#include "netuser.h"

NetUser::NetUser(NetUserRole r, Player *lp, int ui) 
    : role(r),
    localPlayer(lp),\
    userID(ui)
{
    isConnected = true;
}

NetUser::~NetUser()
{

}

uniqueUserID NetUser::GetUniqueUserID() const
{
    return userID;
}

std::string NetUser::GetUserName() const
{
    return userName;
}

Player *NetUser::GetLocalPlayer() const
{
    return localPlayer;
}

NetUserRole NetUser::GetNetRole() const
{
    return role;
}