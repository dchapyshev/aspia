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

#include "base/peer/router_user.h"

#include "base/crypto/secure_string.h"
#include "proto/router_admin.h"

//--------------------------------------------------------------------------------------------------
// static
RouterUser RouterUser::create(const QString& name, const SecureString& password)
{
    User base = User::create(name, password);
    if (!base.isValid())
        return RouterUser();

    RouterUser user;
    static_cast<User&>(user) = std::move(base);
    return user;
}

//--------------------------------------------------------------------------------------------------
// static
RouterUser RouterUser::parseFrom(const proto::router::User& serialized_user)
{
    RouterUser user;

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
proto::router::User RouterUser::serialize() const
{
    proto::router::User user;

    user.set_entry_id(entry_id);
    user.set_name(name.toStdString());
    user.set_group(group.toStdString());
    user.set_salt(salt.toStdString());
    user.set_verifier(verifier.toStdString());
    user.set_sessions(sessions);
    user.set_flags(flags);

    return user;
}
