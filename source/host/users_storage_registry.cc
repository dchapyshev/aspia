//
// PROJECT:         Aspia
// FILE:            host/users_storage_registry.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/users_storage_registry.h"

#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/registry.h"
#include "base/logging.h"
#include "protocol/authorization.h"

namespace aspia {

namespace {

constexpr wchar_t kUsersKeyPath[] = L"SOFTWARE\\Aspia\\Users";
constexpr wchar_t kUserPassword[] = L"Password";
constexpr wchar_t kUserFlags[] = L"Flags";
constexpr wchar_t kUserSessions[] = L"Sessions";

std::wstring GetUserKeyPath(const std::wstring& username)
{
    return StringPrintf(L"%s\\%s", kUsersKeyPath, username.c_str());
}

bool ReadUserInfo(UsersStorage::User& user)
{
    if (!IsValidUserName(user.name))
    {
        LOG(LS_WARNING) << "Invalid user name";
        return false;
    }

    RegistryKey key(HKEY_LOCAL_MACHINE, GetUserKeyPath(user.name).c_str(), KEY_READ);
    if (!key.IsValid())
        return false;

    LONG status = key.ReadValueBIN(kUserPassword, &user.password);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to read user's password: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    if (!IsValidPasswordHash(user.password))
    {
        LOG(LS_WARNING) << "Invalid user's password";
        return false;
    }

    DWORD flags;
    status = key.ReadValueDW(kUserFlags, &flags);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to read user's flags: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    DWORD sessions;
    status = key.ReadValueDW(kUserSessions, &sessions);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to read user's sessions: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    user.flags = flags;
    user.sessions = sessions;

    return true;
}

bool WriteUserInfo(const UsersStorage::User& user)
{
    if (!IsValidUserName(user.name))
    {
        LOG(LS_WARNING) << "Invalid user's name";
        return false;
    }

    if (!IsValidPasswordHash(user.password))
    {
        LOG(LS_WARNING) << "Invalid user's password";
        return false;
    }

    RegistryKey key(HKEY_LOCAL_MACHINE, GetUserKeyPath(user.name).c_str(), KEY_ALL_ACCESS);
    if (!key.IsValid())
        return false;

    LONG status = key.WriteValue(kUserPassword,
                                 user.password.c_str(),
                                 static_cast<DWORD>(user.password.size()),
                                 REG_BINARY);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to write user's password: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    status = key.WriteValue(kUserFlags, static_cast<DWORD>(user.flags));
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to write user's flags: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    status = key.WriteValue(kUserSessions, static_cast<DWORD>(user.sessions));
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to write user's sessions: "
                        << SystemErrorCodeToString(status);
        return false;
    }

    return true;
}

} // namespace

UsersStorage::UserList UsersStorageRegistry::ReadUserList() const
{
    UserList user_list;

    for (RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kUsersKeyPath); iter.Valid(); ++iter)
    {
        User user;
        user.name = iter.Name();

        if (ReadUserInfo(user))
            user_list.push_back(user);
    }

    return user_list;
}

bool UsersStorageRegistry::ReadUser(User& user) const
{
    return ReadUserInfo(user);
}

bool UsersStorageRegistry::AddUser(const User& user)
{
    return WriteUserInfo(user);
}

bool UsersStorageRegistry::ModifyUser(const User& user)
{
    return WriteUserInfo(user);
}

bool UsersStorageRegistry::RemoveUser(const std::wstring& name)
{
    LONG status = RegDeleteKeyW(HKEY_LOCAL_MACHINE, GetUserKeyPath(name).c_str());
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unabel to delete user: " << SystemErrorCodeToString(status);
        return false;
    }

    return true;
}

} // namespace aspia
