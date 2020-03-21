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

#ifndef ROUTER__DATABASE_H
#define ROUTER__DATABASE_H

#include "router/user.h"

namespace router {

class Database
{
public:
    virtual ~Database() = default;

    virtual UserList userList() const = 0;
    virtual bool addUser(const User& user) = 0;
    virtual bool removeUser(std::u16string_view name) = 0;
    virtual std::string id(std::string_view key) const = 0;
};

} // namespace router

#endif // ROUTER__DATABASE_H
