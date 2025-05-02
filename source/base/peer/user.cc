//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace base {

namespace {

const size_t kSaltSize = 64; // In bytes.
const char kDefaultGroup[] = "4096";

//--------------------------------------------------------------------------------------------------
bool isValidUserNameChar(QChar username_char)
{
    if (username_char.isLetterOrNumber())
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

//--------------------------------------------------------------------------------------------------
// static
bool User::isValidUserName(const QString& username)
{
    QString::size_type length = username.length();

    if (!length || length > kMaxUserNameLength)
        return false;

    for (QString::size_type i = 0; i < length; ++i)
    {
        if (!isValidUserNameChar(username[i]))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool User::isValidPassword(const QString& password)
{
    QString::size_type length = password.length();

    if (length < kMinPasswordLength || length > kMaxPasswordLength)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool User::isSafePassword(const QString& password)
{
    QString::size_type length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (QString::size_type i = 0; i < length; ++i)
    {
        QChar character = password.at(i);

        if (character.isUpper())
            has_upper = true;

        if (character.isLower())
            has_lower = true;

        if (character.isDigit())
            has_digit = true;
    }

    return has_upper && has_lower && has_digit;
}

//--------------------------------------------------------------------------------------------------
// static
User User::create(const QString& name, const QString& password)
{
    if (name.isEmpty() || password.isEmpty())
    {
        LOG(LS_ERROR) << "Empty user name or password";
        return User();
    }

    std::optional<SrpNgPair> Ng_pair = pairByGroup(kDefaultGroup);
    if (!Ng_pair.has_value())
    {
        LOG(LS_ERROR) << "Pair not found for group: " << kDefaultGroup;
        return User();
    }

    User user;
    user.name = name;
    user.group = kDefaultGroup;
    user.salt = Random::byteArray(kSaltSize);

    BigNum s = BigNum::fromByteArray(user.salt);
    BigNum N = BigNum::fromStdString(Ng_pair->first);
    BigNum g = BigNum::fromStdString(Ng_pair->second);
    BigNum v = SrpMath::calc_v(name, password, s, N, g);

    user.verifier = v.toByteArray();
    if (user.verifier.isEmpty())
    {
        LOG(LS_ERROR) << "Empty verifier";
        return User();
    }

    return user;
}

//--------------------------------------------------------------------------------------------------
bool User::isValid() const
{
    return !name.isEmpty() && !salt.isEmpty() && !group.isEmpty() && !verifier.isEmpty();
}

//--------------------------------------------------------------------------------------------------
// static
User User::parseFrom(const proto::User& serialized_user)
{
    User user;

    user.entry_id = serialized_user.entry_id();
    user.name     = QString::fromStdString(serialized_user.name());
    user.group    = QString::fromStdString(serialized_user.group());
    user.salt     = QByteArray::fromStdString(serialized_user.salt());
    user.verifier = QByteArray::fromStdString(serialized_user.verifier());
    user.sessions = serialized_user.sessions();
    user.flags    = serialized_user.flags();

    return user;
}

//--------------------------------------------------------------------------------------------------
proto::User User::serialize() const
{
    proto::User user;

    user.set_entry_id(entry_id);
    user.set_name(name.toStdString());
    user.set_group(group.toStdString());
    user.set_salt(salt.toStdString());
    user.set_verifier(verifier.toStdString());
    user.set_sessions(sessions);
    user.set_flags(flags);

    return user;
}

} // namespace base
