//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user_list.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_LIST_H
#define _ASPIA_HOST__HOST_USER_LIST_H

#include "host/host_user.h"

#include <list>

namespace aspia {

class HostUserList
{
public:
    HostUserList();
    ~HostUserList() = default;

    void Load();
    void Store();

private:
    std::list<HostUser> list_;

    DISALLOW_COPY_AND_ASSIGN(HostUserList);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_LIST_H
