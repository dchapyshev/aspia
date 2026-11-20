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

#ifndef ROUTER_STORAGE_DATABASE_H
#define ROUTER_STORAGE_DATABASE_H

#include "base/sql/sql_database.h"

class Database
{
public:
    // A default-constructed Database is not connected; open() (or instance(), which opens the
    // per-thread connection itself) must be called first.
    Database() = default;
    ~Database() = default;

    // Returns the per-thread cached Database. First call on a given thread opens the
    // connection and ensures the schema; subsequent calls are O(1). Connection lives until
    // the thread exits.
    static Database& instance();

    static QString filePath();

    // Opens (creating when absent) the database at |file_path| and ensures the schema.
    // instance() opens the per-thread connection at filePath(); this entry exists so the tests
    // can run every path against an isolated temporary database.
    bool open(const QString& file_path);

    bool isValid() const;

private:
    friend class DatabaseStore;

    bool openDatabase();

    mutable SqlDatabase db_;

    Q_DISABLE_COPY_MOVE(Database)
};

#endif // ROUTER_STORAGE_DATABASE_H
