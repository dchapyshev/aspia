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

#include "host/host_user_list.h"

#include "base/peer/domain_auth_limiter.h"
#include "host/database.h"
#include "host/host_domain_authenticator.h"

//--------------------------------------------------------------------------------------------------
HostUserList::HostUserList(Database& database)
    : database_(database),
      domain_auth_limiter_(std::make_shared<DomainAuthLimiter>())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
User HostUserList::find(const QString& username) const
{
    User user(database_.findUser(username));
    if (user.isValid())
        return user;

    if (one_time_user_.isValid() && one_time_user_.name.compare(username, Qt::CaseInsensitive) == 0)
        return one_time_user_;

    return User();
}

//--------------------------------------------------------------------------------------------------
QByteArray HostUserList::seedKey() const
{
    return database_.seedKey();
}

//--------------------------------------------------------------------------------------------------
void HostUserList::setSeedKey(const QByteArray& seed_key)
{
    database_.setSeedKey(seed_key);
}

//--------------------------------------------------------------------------------------------------
void HostUserList::setOneTimeUser(const User& user)
{
    one_time_user_ = user;
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<const DomainAuthenticator> HostUserList::domainAuthenticator() const
{
    if (!database_.isDomainAuthEnabled())
        return nullptr;

    // A snapshot of the configuration: the object is used on a pool thread and must not touch the
    // database, which belongs to this thread.
    return std::make_shared<HostDomainAuthenticator>(
        database_.domainGroups(), database_.domainPamService());
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<DomainAuthLimiter> HostUserList::domainAuthLimiter() const
{
    return domain_auth_limiter_;
}
