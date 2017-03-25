//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_H
#define _ASPIA_HOST__HOST_USER_H

#include "aspia_config.h"

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "proto/proto.pb.h"

namespace aspia {

class HostUser
{
public:
    HostUser();
    ~HostUser() = default;

    const std::string& UserName() const;
    void SetUserName(const std::string& username);

private:
    std::string username_;
    std::string password_hash_;

    uint32_t session_types_; // –азрешенные возможности дл€ пользовател€.

    DISALLOW_COPY_AND_ASSIGN(HostUser);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_H
