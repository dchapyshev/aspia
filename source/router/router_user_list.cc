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

#include "router/router_user_list.h"

#include "base/logging.h"
#include "proto/router.h"
#include "router/database.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Expands the session mask according to the privilege hierarchy: an administrator implies a manager
// and a client, a manager implies a client.
quint32 expandSessionTypes(quint32 sessions)
{
    if (sessions & proto::router::SESSION_TYPE_ADMIN)
        sessions |= proto::router::SESSION_TYPE_MANAGER | proto::router::SESSION_TYPE_CLIENT;
    if (sessions & proto::router::SESSION_TYPE_MANAGER)
        sessions |= proto::router::SESSION_TYPE_CLIENT;
    return sessions;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterUserList::~RouterUserList() = default;

//--------------------------------------------------------------------------------------------------
// static
SharedPointer<RouterUserList> RouterUserList::open()
{
    if (!Database::instance().isValid())
    {
        LOG(ERROR) << "Unable to open database";
        return nullptr;
    }

    return SharedPointer<RouterUserList>(new RouterUserList());
}

//--------------------------------------------------------------------------------------------------
User RouterUserList::find(const QString& username) const
{
    User user = Database::instance().findUser(username);
    user.sessions = expandSessionTypes(user.sessions);
    return user;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterUserList::seedKey() const
{
    return seed_key_;
}

//--------------------------------------------------------------------------------------------------
void RouterUserList::setSeedKey(const QByteArray& seed_key)
{
    seed_key_ = seed_key;
}
