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

#include "client/storage/database.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/database_store.h"

//--------------------------------------------------------------------------------------------------
// static
QList<Database::Info> Database::list()
{
    QList<Info> result;

    const QString dir_path = directoryPath();
    if (dir_path.isEmpty())
        return result;

    const QDir dir(dir_path);
    const QStringList file_names = dir.entryList({ "*.db3" }, QDir::Files, QDir::Name);

    for (const QString& file_name : file_names)
    {
        const QString id = QFileInfo(file_name).completeBaseName();
        if (QUuid::fromString(id).toString(QUuid::WithoutBraces) != id)
            continue;

        Info& info = result.emplaceBack();
        info.id = id;

        const QString file_path = dir.filePath(file_name);

        SqlDatabase db;
        if (db.open(file_path, SqlDatabase::OpenMode::READ_ONLY))
        {
            SqlQuery query(db, "SELECT name, value FROM settings WHERE name IN (?, ?, ?)");
            query.addText(std::string_view(DatabaseStore::kSettingName));
            query.addText(std::string_view(DatabaseStore::kSettingAutoUnlock));
            query.addText(std::string_view(DatabaseStore::kSettingVerifier));

            bool has_password = false;

            SqlQuery::StepResult step = query.next();
            while (step == SqlQuery::StepResult::ROW)
            {
                const std::string_view name = query.columnTextView(0);

                if (name == DatabaseStore::kSettingName)
                    info.name = query.columnText(1);
                else if (name == DatabaseStore::kSettingAutoUnlock)
                    info.auto_unlock = !query.columnTextView(1).empty();
                else
                    has_password = !query.columnTextView(1).empty();

                step = query.next();
            }

            if (step == SqlQuery::StepResult::FAILED)
                LOG(ERROR) << "Unable to read" << file_path << ":" << db.lastError();
            else
                info.has_password = has_password;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QString Database::readSetting(const QString& name, bool* found) const
{
    if (found)
        *found = false;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    SqlQuery query(db_, "SELECT value FROM settings WHERE name=?");
    query.addText(name);

    if (query.next() != SqlQuery::StepResult::ROW)
        return QString();

    if (found)
        *found = true;

    return query.columnText(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(name);
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
