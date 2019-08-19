//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/user.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"

namespace host {

namespace {

const size_t kUserSaltSize = 64; // In bytes.

} // namespace

// static
User User::create(std::u16string_view name, std::u16string_view password)
{
    User user;

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
        return User();

    return user;
}

bool User::isValid() const
{
    return !name.empty() && !salt.empty() && !number.empty() &&
           !generator.empty() && !verifier.empty();
}

void UserList::add(const User& user)
{
    if (user.isValid())
        list_.push_back(user);
}

void UserList::remove(std::u16string_view username)
{
    size_t index = find(username);
    if (index != -1)
        remove(index);
}

void UserList::remove(size_t index)
{
    DCHECK(hasIndex(index));
    list_.erase(list_.begin() + index);
}

void UserList::update(size_t index, const User& user)
{
    DCHECK(hasIndex(index));
    list_[index] = user;
}

void UserList::merge(const UserList& user_list)
{
    for (const auto& user : user_list.list_)
        add(user);
}

size_t UserList::find(std::u16string_view username) const
{
    for (size_t i = 0; i < list_.size(); ++i)
    {
        const User& user = list_.at(i);

        if ((base::compareCaseInsensitive(user.name, username) == 0) &&
            (user.flags & User::ENABLED))
        {
            return i;
        }
    }

    return -1;
}

const User& UserList::at(size_t index) const
{
    DCHECK(hasIndex(index));
    return list_.at(index);
}

void UserList::setSeedKey(const base::ByteArray& seed_key)
{
    seed_key_ = seed_key;
}

bool UserList::hasIndex(size_t index) const
{
    return index < list_.size();
}

} // namespace host
