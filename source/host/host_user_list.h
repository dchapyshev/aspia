//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user_list.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_USER_LIST_H
#define _ASPIA_HOST__HOST_USER_LIST_H

#include "base/macros.h"
#include "proto/host_user.pb.h"

#include <memory>

namespace aspia {

class HostUserList
{
public:
    HostUserList() = default;
    ~HostUserList();

    bool LoadFromStorage();
    bool SaveToStorage();

    int size() const;
    proto::HostUser* mutable_host_user(int index);
    const proto::HostUser& host_user(int index) const;

    void Add(std::unique_ptr<proto::HostUser> user);
    void Delete(int index);

    bool IsUniqueUserName(const std::wstring& username) const;

    static bool IsValidUserName(const std::wstring& username);
    static bool IsValidPassword(const std::wstring& password);
    static bool IsValidUser(const proto::HostUser& user);

    static bool CreatePasswordHash(const std::string& password,
                                   std::string& password_hash);

private:
    bool IsValidUserList();

    proto::HostUserList list_;

    DISALLOW_COPY_AND_ASSIGN(HostUserList);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_USER_LIST_H
