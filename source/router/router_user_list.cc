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

//--------------------------------------------------------------------------------------------------
RouterUserList::RouterUserList(Database&& db)
    : db_(std::move(db))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
RouterUserList::~RouterUserList() = default;

//--------------------------------------------------------------------------------------------------
// static
SharedPointer<RouterUserList> RouterUserList::open()
{
    Database db = Database::open();
    if (!db.isValid())
    {
        LOG(ERROR) << "Unable to open database";
        return nullptr;
    }

    return SharedPointer<RouterUserList>(new RouterUserList(std::move(db)));
}

//--------------------------------------------------------------------------------------------------
User RouterUserList::find(const QString& username) const
{
    return db_.findUser(username);
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
