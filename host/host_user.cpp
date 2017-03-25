//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_user.h"

namespace aspia {

HostUser::HostUser() :
    session_types_(proto::SessionType::SESSION_NONE)
{
    // Nothing
}

const std::string& HostUser::UserName() const
{
    return username_;
}

void HostUser::SetUserName(const std::string& username)
{
    username_ = username;
}

} // namespace aspia
