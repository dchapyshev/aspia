//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_user_utils.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_user_utils.h"
#include "base/unicode.h"
#include "base/logging.h"
#include "base/path.h"

#include <filesystem>
#include <fstream>
#include <memory>

namespace aspia {

static const size_t kMaximumUserNameLength = 64;
static const size_t kMinimumPasswordLength = 6;
static const size_t kMaximumPasswordLength = 64;
static const size_t kPasswordHashLength = 64; // 512 bits

static const size_t kMaximumUserListSize = 10 * 1024 * 1024; // 10MB

static bool IsValidUserNameChar(wchar_t username_char)
{
    if (iswalpha(username_char))
        return true;

    if (iswdigit(username_char))
        return true;

    if (username_char == L'.' || username_char == L'_' || username_char == L'-')
        return true;

    return false;
}

bool IsValidUserName(const std::wstring& username)
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

bool IsValidPassword(const std::wstring& password)
{
    size_t length = password.length();

    if (length < kMinimumPasswordLength || length > kMaximumPasswordLength)
        return false;

    return true;
}

bool IsValidPasswordHash(const std::string& password_hash)
{
    return password_hash.length() == kPasswordHashLength;
}

bool IsValidUser(const proto::HostUser& user)
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

bool IsValidUserList(const proto::HostUserList& list)
{
    for (int i = 0; i < list.user_list_size(); ++i)
    {
        if (!IsValidUser(list.user_list(i)))
            return false;
    }

    return true;
}

static bool GetUserListDirectoryPath(std::string& path)
{
    if (!GetPath(PathKey::DIR_COMMON_APP_DATA, path))
        return false;

    path.append("/Aspia/Remote Desktop");
    return true;
}

static bool GetUserListFilePath(std::string& path)
{
    if (!GetUserListDirectoryPath(path))
        return false;

    path.append("/userlist.dat");
    return true;
}

bool WriteUserList(const proto::HostUserList& list)
{
    if (!IsValidUserList(list))
    {
        LOG(ERROR) << "User list contains incorrect entries";
        return false;
    }

    std::string dir_path;
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
            LOG(ERROR) << "Path '" << dir_path << "' exist, not it is not a directory";
            return false;
        }
    }

    std::string file_path;
    if (!GetUserListFilePath(file_path))
        return false;

    std::ofstream file_stream;

    file_stream.open(file_path, std::ofstream::binary);
    if (!file_stream.is_open())
    {
        LOG(ERROR) << "Unable to create (or open) file: " << file_path;
        return false;
    }

    std::string string = list.SerializeAsString();

    file_stream.write(string.c_str(), string.size());
    file_stream.close();

    return true;
}

bool ReadUserList(proto::HostUserList& list)
{
    std::string file_path;

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
    file_stream.close();

    if (!list.ParseFromString(string))
    {
        LOG(ERROR) << "User list corrupted";
        return false;
    }

    if (!IsValidUserList(list))
    {
        LOG(ERROR) << "User list contains incorrect entries";
        return false;
    }

    return true;
}

bool IsUniqueUserName(const std::wstring& username)
{
    proto::HostUserList list;

    if (!ReadUserList(list))
        return true;

    std::string username_utf8;
    CHECK(UNICODEtoUTF8(username, username_utf8));

    for (int i = 0; i < list.user_list_size(); ++i)
    {
        if (list.user_list(i).username() == username_utf8)
            return false;
    }

    return true;
}

} // namespace aspia
