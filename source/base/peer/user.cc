//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/peer/user.h"

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/srp_constants.h"
#include "base/crypto/srp_math.h"
#include "base/strings/unicode.h"

#include <cwctype>

namespace base {

namespace {

const size_t kSaltSize = 64; // In bytes.
const char kDefaultGroup[] = "4096";

bool isValidUserNameChar(char16_t username_char)
{
    if (std::iswalnum(username_char))
        return true;

    if (username_char == '.' ||
        username_char == '_' ||
        username_char == '-' ||
        username_char == '@')
    {
        return true;
    }

    return false;
}

} // namespace

// static
const User User::kInvalidUser;

// static
bool User::isValidUserName(std::u16string_view username)
{
    size_t length = username.length();

    if (!length || length > kMaxUserNameLength)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (!isValidUserNameChar(username[i]))
            return false;
    }

    return true;
}

// static
bool User::isValidPassword(std::u16string_view password)
{
    size_t length = password.length();

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

// static
bool User::isSafePassword(std::u16string_view password)
{
    size_t length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (size_t i = 0; i < length; ++i)
    {
        char16_t character = password[i];

        if (std::iswupper(character))
            has_upper = true;

        if (std::iswlower(character))
            has_lower = true;

        if (std::iswdigit(character))
            has_digit = true;
    }

    return has_upper && has_lower && has_digit;
}

// static
User User::create(std::u16string_view name, std::u16string_view password)
{
    if (name.empty() || password.empty())
        return User();

    std::optional<SrpNgPair> Ng_pair = pairByGroup(kDefaultGroup);
    if (!Ng_pair.has_value())
        return User();

    User user;
    user.name = name;
    user.group = kDefaultGroup;
    user.salt = Random::byteArray(kSaltSize);

    BigNum s = BigNum::fromByteArray(user.salt);
    BigNum N = BigNum::fromStdString(Ng_pair->first);
    BigNum g = BigNum::fromStdString(Ng_pair->second);
    BigNum v = SrpMath::calc_v(name, password, s, N, g);

    user.verifier = v.toByteArray();
    if (user.verifier.empty())
        return User();

    return user;
}

bool User::isValid() const
{
    return !name.empty() && !salt.empty() && !group.empty() && !verifier.empty();
}

// static
User User::parseFrom(const proto::User& serialized_user)
{
    User user;

    user.entry_id = serialized_user.entry_id();
    user.name     = utf16FromUtf8(serialized_user.name());
    user.group    = serialized_user.group();
    user.salt     = fromStdString(serialized_user.salt());
    user.verifier = fromStdString(serialized_user.verifier());
    user.sessions = serialized_user.sessions();
    user.flags    = serialized_user.flags();

    return user;
}

proto::User User::serialize() const
{
    proto::User user;

    user.set_entry_id(entry_id);
    user.set_name(utf8FromUtf16(name));
    user.set_group(group);
    user.set_salt(toStdString(salt));
    user.set_verifier(toStdString(verifier));
    user.set_sessions(sessions);
    user.set_flags(flags);

    return user;
}

} // namespace base
