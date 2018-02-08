//
// PROJECT:         Aspia
// FILE:            host/host_user_list.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_user_list.h"

#include <fstream>
#include <memory>

#include "base/strings/unicode.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "crypto/secure_memory.h"
#include "crypto/sha.h"
#include "protocol/authorization.h"

namespace aspia {

namespace {

constexpr size_t kMaximumUserListSize = 10 * 1024 * 1024; // 10MB

} // namespace

HostUserList::~HostUserList()
{
    for (int i = 0; i < list_.user_list_size(); ++i)
    {
        proto::HostUser* user = list_.mutable_user_list(i);

        SecureMemZero(user->mutable_username());
        SecureMemZero(user->mutable_password_hash());

        user->set_session_types(0);
        user->set_enabled(false);
    }
}

bool HostUserList::IsValidUser(const proto::HostUser& user)
{
    if (!IsValidUserNameUTF8(user.username()))
        return false;

    if (!IsValidPasswordHash(user.password_hash()))
        return false;

    return true;
}

bool HostUserList::IsValidUserList()
{
    for (int i = 0; i < list_.user_list_size(); ++i)
    {
        if (!IsValidUser(list_.user_list(i)))
            return false;
    }

    return true;
}

static bool GetUserListDirectoryPath(std::experimental::filesystem::path& path)
{
    if (!GetBasePath(BasePathKey::DIR_COMMON_APP_DATA, path))
        return false;

    path.append(L"aspia");

    return true;
}

static bool GetUserListFilePath(std::experimental::filesystem::path& path)
{
    if (!GetUserListDirectoryPath(path))
        return false;

    path.append(L"userlist.dat");
    return true;
}

bool HostUserList::LoadFromStorage()
{
    std::experimental::filesystem::path file_path;

    if (!GetUserListFilePath(file_path))
        return false;

    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
    {
        LOG(LS_ERROR) << "File not found: " << file_path;
        return false;
    }

    file_stream.seekg(0, file_stream.end);
    std::streamoff size = static_cast<size_t>(file_stream.tellg());
    file_stream.seekg(0);

    // Normal case (empty user list).
    if (!size)
        return true;

    if (size > kMaximumUserListSize)
    {
        LOG(LS_ERROR) << "User list is too large (>10MB)";
        return false;
    }

    std::string string;
    string.resize(static_cast<size_t>(size));

    file_stream.read(string.data(), size);

    if (file_stream.fail())
    {
        LOG(LS_ERROR) << "Unable to read user list";
        return false;
    }

    file_stream.close();

    if (!list_.ParseFromString(string))
    {
        LOG(LS_ERROR) << "User list corrupted";
        return false;
    }

    if (!IsValidUserList())
    {
        LOG(LS_ERROR) << "User list contains incorrect entries";
        return false;
    }

    return true;
}

bool HostUserList::SaveToStorage()
{
    if (!IsValidUserList())
    {
        LOG(LS_ERROR) << "User list contains incorrect entries";
        return false;
    }

    std::experimental::filesystem::path dir_path;
    if (!GetUserListDirectoryPath(dir_path))
        return false;

    if (!std::experimental::filesystem::exists(dir_path))
    {
        std::error_code code;

        if (!std::experimental::filesystem::create_directories(dir_path, code))
        {
            LOG(LS_ERROR) << "Unable to create directory: '" << dir_path
                          << "'. Error: " << code.message();
            return false;
        }
    }
    else
    {
        if (!std::experimental::filesystem::is_directory(dir_path))
        {
            LOG(LS_ERROR) << "Path '" << dir_path
                          << "' exist, not it is not a directory";
            return false;
        }
    }

    std::experimental::filesystem::path file_path;
    if (!GetUserListFilePath(file_path))
        return false;

    std::ofstream file_stream;

    file_stream.open(file_path, std::ofstream::binary);
    if (!file_stream.is_open())
    {
        LOG(LS_ERROR) << "Unable to create (or open) file: " << file_path;
        return false;
    }

    std::string string = list_.SerializeAsString();

    file_stream.write(string.c_str(), string.size());
    if (file_stream.fail())
    {
        LOG(LS_ERROR) << "Unable to write user list";
        return false;
    }

    file_stream.close();

    return true;
}

int HostUserList::size() const
{
    return list_.user_list_size();
}

proto::HostUser* HostUserList::mutable_host_user(int index)
{
    return list_.mutable_user_list(index);
}

const proto::HostUser& HostUserList::host_user(int index) const
{
    return list_.user_list(index);
}

void HostUserList::Add(std::unique_ptr<proto::HostUser> user)
{
    list_.mutable_user_list()->AddAllocated(user.release());
}

void HostUserList::Delete(int index)
{
    list_.mutable_user_list()->DeleteSubrange(index, 1);
}

bool HostUserList::IsUniqueUserName(const std::wstring& username) const
{
    std::string username_utf8;
    CHECK(UNICODEtoUTF8(username, username_utf8));

    const int size = list_.user_list_size();

    for (int i = 0; i < size; ++i)
    {
        if (list_.user_list(i).username() == username_utf8)
            return false;
    }

    return true;
}

} // namespace aspia
