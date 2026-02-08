//
// SmartCafe Project
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

#ifndef ROUTER_USER_LIST_DB_H
#define ROUTER_USER_LIST_DB_H

#include "base/peer/user_list_base.h"

namespace router {

class Database;
class DatabaseFactory;

class UserListDb final : public base::UserListBase
{
public:
    ~UserListDb() final;

    static std::unique_ptr<UserListDb> open(const DatabaseFactory& factory);

    // base::UserListBase implementation.
    void add(const base::User& user) final;
    base::User find(const QString& username) const final;
    const QByteArray& seedKey() const final;
    void setSeedKey(const QByteArray& seed_key) final;
    QVector<base::User> list() const final;

private:
    explicit UserListDb(std::unique_ptr<Database> db);

    std::unique_ptr<Database> db_;
    QByteArray seed_key_;

    Q_DISABLE_COPY(UserListDb)
};

} // namespace router

#endif // ROUTER_USER_LIST_DB_H
