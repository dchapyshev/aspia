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

#ifndef CLIENT_STORAGE_DATABASE_MIGRATION_H
#define CLIENT_STORAGE_DATABASE_MIGRATION_H

#include <QString>

class Database;
class SqlDatabase;

class DatabaseMigration
{
public:
    enum class Result { NOT_NEEDED, DONE, FAILED };

    static QString oldFilePath();

    static Result migrate(const QString& old_file_path, const QString& dir_path);
    static bool finish(Database& db, const QString& old_file_path);

private:
    static bool build(const QString& old_file_path, const QString& dir_path);
    static bool buildDatabase(SqlDatabase& old_db, const QString& file_path, qint64 router_id,
                              const QString& router_guid);

    DatabaseMigration() = delete;
    ~DatabaseMigration() = delete;
};

#endif // CLIENT_STORAGE_DATABASE_MIGRATION_H
