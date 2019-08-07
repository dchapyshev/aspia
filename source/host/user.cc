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
#include "crypto/random.h"
#include "crypto/srp_constants.h"
#include "crypto/srp_math.h"

namespace host {

namespace {

const size_t kUserSaltSize = 64; // In bytes.

} // namespace

// static
User User::create(const QString& name, const QString& password)
{
    User user;

    user.name = name;
    user.salt = crypto::Random::byteArray(kUserSaltSize);

    user.number = QByteArray(
        reinterpret_cast<const char*>(crypto::kSrpNg_8192.N.data()), crypto::kSrpNg_8192.N.size());
    user.generator = QByteArray(
        reinterpret_cast<const char*>(crypto::kSrpNg_8192.g.data()), crypto::kSrpNg_8192.g.size());

    crypto::BigNum s = crypto::BigNum::fromByteArray(user.salt);
    crypto::BigNum N = crypto::BigNum::fromByteArray(user.number);
    crypto::BigNum g = crypto::BigNum::fromByteArray(user.generator);
    crypto::BigNum v = crypto::SrpMath::calc_v(name, password, s, N, g);

    user.verifier = v.toByteArray();
    if (user.verifier.isEmpty())
        return User();

    return user;
}

bool User::isValid() const
{
    return !name.isEmpty() && !salt.isEmpty() && !number.isEmpty() &&
           !generator.isEmpty() && !verifier.isEmpty();
}

void UserList::add(const User& user)
{
    if (user.isValid())
        list_.push_back(user);
}

void UserList::remove(const QString& username)
{
    int index = find(username);
    if (index != -1)
        list_.removeAt(index);
}

void UserList::remove(int index)
{
    DCHECK(hasIndex(index));

    list_.removeAt(index);
}

void UserList::update(int index, const User& user)
{
    DCHECK(hasIndex(index));

    list_.replace(index, user);
}

int UserList::find(const QString& username) const
{
    for (int i = 0; i < list_.size(); ++i)
    {
        const User& user = list_.at(i);

        if ((user.name.compare(username, Qt::CaseInsensitive) == 0) &&
            (user.flags & User::ENABLED))
        {
            return i;
        }
    }

    return -1;
}

const User& UserList::at(int index) const
{
    DCHECK(hasIndex(index));

    return list_.at(index);
}

void UserList::setSeedKey(const QByteArray& seed_key)
{
    seed_key_ = seed_key;
}

bool UserList::hasIndex(int index) const
{
    return index >= 0 && index < list_.size();
}

} // namespace host
