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

#ifndef ROUTER_DATABASE_FACTORY_SQLITE_H
#define ROUTER_DATABASE_FACTORY_SQLITE_H

#include <QtGlobal>

#include "router/database_factory.h"

namespace router {

class DatabaseFactorySqlite final : public DatabaseFactory
{
public:
    DatabaseFactorySqlite();
    ~DatabaseFactorySqlite() final;

    std::unique_ptr<Database> createDatabase() const final;
    std::unique_ptr<Database> openDatabase() const final;

private:
    Q_DISABLE_COPY(DatabaseFactorySqlite)
};

} // namespace router

#endif // ROUTER_DATABASE_FACTORY_SQLITE_H
