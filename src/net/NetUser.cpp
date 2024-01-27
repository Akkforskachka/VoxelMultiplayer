#include "NetUser.h"

NetUser::NetUser(NetUserRole r, int ui) 
    : role(r),
    userID(ui)
{

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

NetUserRole NetUser::GetNetRole() const
{
    return role;
}