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

#ifndef CLIENT_JSON_BACKUP_H
#define CLIENT_JSON_BACKUP_H

#include <QCoreApplication>
#include <QString>

class QWidget;

class JsonBackup
{
    Q_DECLARE_TR_FUNCTIONS(JsonBackup)

public:
    // Exports the local address book (groups, computers, routers) into a JSON file at |file_path|.
    // Shows password dialog and error/success messages on |parent|. Returns true on success.
    static bool exportToFile(QWidget* parent, const QString& file_path);

    // Imports the address book from |file_path| (JSON format) into the local database.
    // Shows password dialog and error/success messages on |parent|.
    // Returns true if at least one item was imported.
    static bool importFromFile(QWidget* parent, const QString& file_path);

private:
    Q_DISABLE_COPY_MOVE(JsonBackup)
};

#endif // CLIENT_JSON_BACKUP_H
