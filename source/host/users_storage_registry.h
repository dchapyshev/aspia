//
// PROJECT:         Aspia
// FILE:            host/users_storage_registry.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__USERS_STORAGE_REGISTRY_H
#define _ASPIA_HOST__USERS_STORAGE_REGISTRY_H

#include "base/macros.h"
#include "host/users_storage.h"

namespace aspia {

class UsersStorageRegistry : public UsersStorage
{
public:
    UsersStorageRegistry() = default;

    UserList ReadUserList() const override;
    bool ReadUser(User& user) const override;
    bool AddUser(const User& user) override;
    bool ModifyUser(const User& user) override;
    bool RemoveUser(const std::wstring& name) override;

private:
    DISALLOW_COPY_AND_ASSIGN(UsersStorageRegistry);
};

} // namespace aspia

#endif // _ASPIA_HOST__USERS_STORAGE_REGISTRY_H
