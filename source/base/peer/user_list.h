//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_PEER_USER_LIST_BASE_H
#define BASE_PEER_USER_LIST_BASE_H

#include <QVector>

#include <memory>

#include "base/peer/user.h"

class DomainAuthenticator;
class DomainAuthLimiter;

class UserList
{
public:
    virtual ~UserList() = default;

    virtual User find(const QString& username) const = 0;
    virtual QByteArray seedKey() const = 0;
    virtual void setSeedKey(const QByteArray& seed_key) = 0;
    virtual void setOneTimeUser(const User& /* user */) {}

    // Returns the authenticator for operating system / domain accounts (IDENTIFY_PASSWORD) with
    // the current configuration, or nullptr when the method is disabled. Called on the thread of
    // the user list for every connection; the returned object is then used on a pool thread.
    virtual std::shared_ptr<const DomainAuthenticator> domainAuthenticator() const { return nullptr; }

    // Returns the limiter of failed operating system / domain logons shared by all connections of
    // the server, or nullptr for no limiting.
    virtual std::shared_ptr<DomainAuthLimiter> domainAuthLimiter() const { return nullptr; }
};

#endif // BASE_PEER_USER_LIST_BASE_H
