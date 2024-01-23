#include "netuser.h"

NetUser::NetUser(NetUserRole r, Player *lp, int ui) 
    : role(r),
    localPlayer(lp),\
    userID(ui)
{

}

NetUser::~NetUser()
{

}

Player *NetUser::GetLocalPlayer() const
{
    return localPlayer;
}

NetUserRole NetUser::GetNetRole() const
{
    return role;
}