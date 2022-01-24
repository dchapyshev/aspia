//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__DATABASE_FACTORY_SQLITE_H
#define ROUTER__DATABASE_FACTORY_SQLITE_H

#include "base/macros_magic.h"
#include "router/database_factory.h"

namespace router {

class DatabaseFactorySqlite : public DatabaseFactory
{
public:
    DatabaseFactorySqlite();
    ~DatabaseFactorySqlite();

    std::unique_ptr<Database> createDatabase() const override;
    std::unique_ptr<Database> openDatabase() const override;

private:
    DISALLOW_COPY_AND_ASSIGN(DatabaseFactorySqlite);
};

} // namespace router

#endif // ROUTER__DATABASE_FACTORY_SQLITE_H
