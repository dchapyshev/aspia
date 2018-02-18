//
// PROJECT:         Aspia
// FILE:            host/users_storage.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__USERS_STORAGE_H
#define _ASPIA_HOST__USERS_STORAGE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aspia {

class UsersStorage
{
public:
    virtual ~UsersStorage() = default;

    static std::unique_ptr<UsersStorage> Open();

    enum UserFlags { USER_FLAG_ENABLED = 1 };

    struct User
    {
        std::wstring name;
        std::string password;
        uint32_t flags = 0;
        uint32_t sessions = 0;
    };

    using UserList = std::vector<User>;

    virtual UserList ReadUserList() const = 0;
    virtual bool ReadUser(User& user) const = 0;
    virtual bool AddUser(const User& user) = 0;
    virtual bool ModifyUser(const User& user) = 0;
    virtual bool RemoveUser(const std::wstring& name) = 0;
};

} // namespace aspia

#endif // _ASPIA_HOST__USERS_STORAGE_H
