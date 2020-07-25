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

#include "base/net/user.h"

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/srp_constants.h"
#include "base/crypto/srp_math.h"
#include "base/strings/string_util.h"

#include <cwctype>

namespace base {

namespace {

const User kInvalidUser;
const size_t kSaltSize = 64; // In bytes.
const char kDefaultGroup[] = "8192";

bool isValidUserNameChar(char16_t username_char)
{
    if (std::iswalnum(username_char))
        return true;

    if (username_char == '.' ||
        username_char == '_' ||
        username_char == '-')
    {
        return true;
    }

    return false;
}

} // namespace

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

    std::optional<SrpNgPair> Ng_pair = base::pairByGroup(kDefaultGroup);
    if (!Ng_pair.has_value())
        return User();

    User user;
    user.name = name;
    user.group = kDefaultGroup;
    user.salt = base::Random::byteArray(kSaltSize);

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

void UserList::add(const User& user)
{
    if (user.isValid())
        list_.emplace_back(user);
}

void UserList::add(User&& user)
{
    if (user.isValid())
        list_.emplace_back(std::move(user));
}

void UserList::merge(const UserList& user_list)
{
    for (const auto& user : user_list.list_)
        add(user);
}

void UserList::merge(UserList&& user_list)
{
    for (auto& user : user_list.list_)
        add(std::move(user));
}

const User& UserList::find(std::u16string_view username) const
{
    const User* user = &kInvalidUser;

    for (const auto& item : list_)
    {
        if (compareCaseInsensitive(username, item.name) == 0)
            user = &item;
    }

    return *user;
}

void UserList::setSeedKey(const ByteArray& seed_key)
{
    seed_key_ = seed_key;
}

void UserList::setSeedKey(ByteArray&& seed_key)
{
    seed_key_ = std::move(seed_key);
}

UserList::Iterator::Iterator(const UserList& list)
    : list_(list.list_),
      pos_(list.list_.cbegin())
{
    // Nothing
}

UserList::Iterator::~Iterator() = default;

const User& UserList::Iterator::user() const
{
    if (isAtEnd())
        return kInvalidUser;

    return *pos_;
}

bool UserList::Iterator::isAtEnd() const
{
    return pos_ == list_.cend();
}

void UserList::Iterator::advance()
{
    ++pos_;
}

} // namespace base
