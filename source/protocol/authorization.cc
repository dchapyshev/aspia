//
// PROJECT:         Aspia
// FILE:            protocol/authorization.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/authorization.h"

#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"
#include "crypto/random.h"
#include "crypto/secure_memory.h"
#include "crypto/sha.h"

namespace aspia {

namespace {

// Number of iterations for hashing a user's password.
constexpr size_t kPasswordHashIterCount = 1000;

constexpr size_t kMaxUserNameLength = 64;
constexpr size_t kMinPasswordLength = 8;
constexpr size_t kMaxPasswordLength = 64;
constexpr size_t kPasswordHashLength = 64; // 512 bits

constexpr size_t kNonceSize = 512;

bool IsValidUserNameChar(wchar_t username_char)
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

} // namespace

bool IsValidUserName(const std::wstring& username)
{
    size_t length = username.length();

    if (!length || length > kMaxUserNameLength)
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

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

std::string CreatePasswordHash(const std::wstring& password)
{
    std::string password_hash = SHA512(password, kPasswordHashIterCount);
    DCHECK(IsValidPasswordHash(password_hash));
    return password_hash;
}

bool IsValidPasswordHash(const std::string& password_hash)
{
    return password_hash.length() == kPasswordHashLength;
}

std::string CreateNonce()
{
    return CreateRandomBuffer(kNonceSize);
}

std::string CreateUserKey(const std::string& password_hash, const std::string& nonce)
{
    DCHECK_EQ(nonce.size(), kNonceSize);

    StreamSHA512 sha512;

    sha512.AppendData(nonce);
    sha512.AppendData(password_hash);

    return sha512.Final();
}

} // namespace aspia
