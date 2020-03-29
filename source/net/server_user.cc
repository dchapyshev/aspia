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

#include "net/server_user.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"

namespace net {

namespace {

const ServerUser kInvalidUser;
const size_t kUserSaltSize = 64; // In bytes.

} // namespace

// static
ServerUser ServerUser::create(std::u16string_view name, std::u16string_view password)
{
    if (name.empty() || password.empty())
        return ServerUser();

    ServerUser user;

    user.name = name;
    user.salt = crypto::Random::byteArray(kUserSaltSize);
    user.number = base::fromStdString(crypto::kSrpNgPair_8192.first);
    user.generator = base::fromStdString(crypto::kSrpNgPair_8192.second);

    crypto::BigNum s = crypto::BigNum::fromByteArray(user.salt);
    crypto::BigNum N = crypto::BigNum::fromByteArray(user.number);
    crypto::BigNum g = crypto::BigNum::fromByteArray(user.generator);
    crypto::BigNum v = crypto::SrpMath::calc_v(name, password, s, N, g);

    user.verifier = v.toByteArray();
    if (user.verifier.empty())
        return ServerUser();

    return user;
}

bool ServerUser::isValid() const
{
    return !name.empty() && !salt.empty() && !number.empty() &&
           !generator.empty() && !verifier.empty();
}

void ServerUserList::add(const ServerUser& user)
{
    if (user.isValid())
        list_.emplace_back(user);
}

void ServerUserList::add(ServerUser&& user)
{
    if (user.isValid())
        list_.emplace_back(std::move(user));
}

void ServerUserList::merge(const ServerUserList& user_list)
{
    for (const auto& user : user_list.list_)
        add(user);
}

void ServerUserList::merge(ServerUserList&& user_list)
{
    for (auto& user : user_list.list_)
        add(std::move(user));
}

const ServerUser& ServerUserList::find(std::u16string_view username) const
{
    const ServerUser* user = &kInvalidUser;

    for (const auto& item : list_)
    {
        if (base::compareCaseInsensitive(username, item.name) == 0)
            user = &item;
    }

    return *user;
}

void ServerUserList::setSeedKey(const base::ByteArray& seed_key)
{
    seed_key_ = seed_key;
}

void ServerUserList::setSeedKey(base::ByteArray&& seed_key)
{
    seed_key_ = std::move(seed_key);
}

ServerUserList::Iterator::Iterator(const ServerUserList& list)
    : list_(list.list_),
      pos_(list.list_.cbegin())
{
    // Nothing
}

ServerUserList::Iterator::~Iterator() = default;

const ServerUser& ServerUserList::Iterator::user() const
{
    if (isAtEnd())
        return kInvalidUser;

    return *pos_;
}

bool ServerUserList::Iterator::isAtEnd() const
{
    return pos_ == list_.cend();
}

void ServerUserList::Iterator::advance()
{
    ++pos_;
}

} // namespace net
