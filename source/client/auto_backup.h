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

#ifndef CLIENT_AUTO_BACKUP_H
#define CLIENT_AUTO_BACKUP_H

#include "client/backup.h"

// Backups of the database made on every start of the application into a directory chosen by the user.
class AutoBackup
{
public:
    enum class Retention
    {
        ONE_WEEK = 0,
        TWO_WEEKS = 1,
        ONE_MONTH = 2,
        SIX_MONTHS = 3,
        ONE_YEAR = 4
    };

    // Writes a new backup of |db| into |directory| and then removes the backups older than
    // |retention|. Nothing is removed unless the new backup was written.
    static Backup::Result run(Database& db, const QString& directory, Retention retention);

private:
    Q_DISABLE_COPY_MOVE(AutoBackup)
};

#endif // CLIENT_AUTO_BACKUP_H
