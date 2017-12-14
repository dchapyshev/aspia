//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user_list.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_user_list.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "base/files/base_paths.h"
#include "crypto/secure_memory.h"
#include "crypto/sha512.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace aspia {

static const size_t kMaximumUserNameLength = 64;
static const size_t kMinimumPasswordLength = 6;
static const size_t kMaximumPasswordLength = 64;
static const size_t kPasswordHashLength = 64; // 512 bits

static const size_t kMaximumUserListSize = 10 * 1024 * 1024; // 10MB

// Number of iterations for hashing a user's password.
static const size_t kPasswordHashIterCount = 100;

HostUserList::~HostUserList()
{
    for (int i = 0; i < list_.user_list_size(); ++i)
    {
        proto::HostUser* user = list_.mutable_user_list(i);

        SecureMemZero(*user->mutable_username());
        SecureMemZero(*user->mutable_password_hash());

        user->set_session_types(0);
        user->set_enabled(false);
    }
}

static bool IsValidPasswordHash(const std::string& password_hash)
{
    return password_hash.size() == kPasswordHashLength;
}

bool HostUserList::IsValidUser(const proto::HostUser& user)
{
    std::wstring username;

    if (!UTF8toUNICODE(user.username(), username))
        return false;

    if (!IsValidUserName(username))
        return false;

    if (!IsValidPasswordHash(user.password_hash()))
        return false;

    return true;
}

// static
bool HostUserList::CreatePasswordHash(const std::string& password,
                                      std::string& password_hash)
{
    bool result = false;

    std::wstring password_unicode = UNICODEfromUTF8(password);
    if (IsValidPassword(password_unicode))
    {
        result = CreateSHA512(password,
                              password_hash,
                              kPasswordHashIterCount);
    }

    SecureMemZero(password_unicode);

    return result;
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

static bool GetUserListDirectoryPath(FilePath& path)
{
    if (!GetBasePath(BasePathKey::DIR_COMMON_APP_DATA, path))
        return false;

    path.append(L"Aspia");
    path.append(L"Remote Desktop");

    return true;
}

static bool GetUserListFilePath(FilePath& path)
{
    if (!GetUserListDirectoryPath(path))
        return false;

    path.append(L"userlist.dat");
    return true;
}

bool HostUserList::LoadFromStorage()
{
    FilePath file_path;

    if (!GetUserListFilePath(file_path))
        return false;

    std::ifstream file_stream;

    file_stream.open(file_path, std::ifstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "File not found: " << file_path;
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
        LOG(ERROR) << "User list is too large (>10MB)";
        return false;
    }

    std::string string;
    string.resize(static_cast<size_t>(size));

    file_stream.read(&string[0], size);

    if (file_stream.fail())
    {
        LOG(ERROR) << "Unable to read user list";
        return false;
    }

    file_stream.close();

    if (!list_.ParseFromString(string))
    {
        LOG(ERROR) << "User list corrupted";
        return false;
    }

    if (!IsValidUserList())
    {
        LOG(ERROR) << "User list contains incorrect entries";
        return false;
    }

    return true;
}

bool HostUserList::SaveToStorage()
{
    if (!IsValidUserList())
    {
        LOG(ERROR) << "User list contains incorrect entries";
        return false;
    }

    FilePath dir_path;
    if (!GetUserListDirectoryPath(dir_path))
        return false;

    if (!std::experimental::filesystem::exists(dir_path))
    {
        std::error_code code;

        if (!std::experimental::filesystem::create_directories(dir_path, code))
        {
            LOG(ERROR) << "Unable to create directory: '" << dir_path
                       << "'. Error: " << code.message();
            return false;
        }
    }
    else
    {
        if (!std::experimental::filesystem::is_directory(dir_path))
        {
            LOG(ERROR) << "Path '" << dir_path
                       << "' exist, not it is not a directory";
            return false;
        }
    }

    FilePath file_path;
    if (!GetUserListFilePath(file_path))
        return false;

    std::ofstream file_stream;

    file_stream.open(file_path, std::ofstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "Unable to create (or open) file: " << file_path;
        return false;
    }

    std::string string = list_.SerializeAsString();

    file_stream.write(string.c_str(), string.size());
    if (file_stream.fail())
    {
        LOG(ERROR) << "Unable to write user list";
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

static bool IsValidUserNameChar(wchar_t username_char)
{
    if (iswalpha(username_char))
        return true;

    if (iswdigit(username_char))
        return true;

    if (username_char == L'.' ||
        username_char == L'_' ||
        username_char == L'-')
    {
        return true;
    }

    return false;
}

// static
bool HostUserList::IsValidUserName(const std::wstring& username)
{
    size_t length = username.length();

    if (!length || length > kMaximumUserNameLength)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (!IsValidUserNameChar(username[i]))
            return false;
    }

    return true;
}

// static
bool HostUserList::IsValidPassword(const std::wstring& password)
{
    size_t length = password.length();

    if (length < kMinimumPasswordLength || length > kMaximumPasswordLength)
        return false;

    return true;
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
