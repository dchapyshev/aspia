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

#ifndef ROUTER__USER_LIST_DB_H
#define ROUTER__USER_LIST_DB_H

#include "base/macros_magic.h"
#include "base/peer/user_list_base.h"

namespace router {

class Database;
class DatabaseFactory;

class UserListDb : public base::UserListBase
{
public:
    ~UserListDb();

    static std::unique_ptr<UserListDb> open(const DatabaseFactory& factory);

    // base::UserListBase implementation.
    void add(const base::User& user) override;
    base::User find(std::u16string_view username) const override;
    const base::ByteArray& seedKey() const override;
    void setSeedKey(const base::ByteArray& seed_key) override;
    base::ScalableVector<base::User> list() const override;

private:
    explicit UserListDb(std::unique_ptr<Database> db);

    std::unique_ptr<Database> db_;
    base::ByteArray seed_key_;

    DISALLOW_COPY_AND_ASSIGN(UserListDb);
};

} // namespace router

#endif // ROUTER__USER_LIST_DB_H
