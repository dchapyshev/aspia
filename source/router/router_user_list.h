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

#ifndef ROUTER_ROUTER_USER_LIST_H
#define ROUTER_ROUTER_USER_LIST_H

#include <memory>

#include "base/peer/user_list.h"
#include "router/database.h"

class RouterUserList final : public UserList
{
public:
    ~RouterUserList() final;

    static std::unique_ptr<RouterUserList> open();

    // UserListBase implementation.
    User find(const QString& username) const final;
    QByteArray seedKey() const final;
    void setSeedKey(const QByteArray& seed_key) final;

private:
    explicit RouterUserList(Database&& db);

    Database db_;
    QByteArray seed_key_;

    Q_DISABLE_COPY_MOVE(RouterUserList)
};

#endif // ROUTER_ROUTER_USER_LIST_H
