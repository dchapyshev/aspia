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

#include "client/auto_backup.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include "base/logging.h"

namespace {

const char kFilePrefix[] = "aspia-backup-";
const char kFileSuffix[] = ".aspia-backup";
const char kTimeFormat[] = "yyyy-MM-dd-HHmmss";

//--------------------------------------------------------------------------------------------------
QDateTime expiryTime(AutoBackup::Retention retention, const QDateTime& now)
{
    switch (retention)
    {
        case AutoBackup::Retention::ONE_WEEK:
            return now.addDays(-7);

        case AutoBackup::Retention::TWO_WEEKS:
            return now.addDays(-14);

        case AutoBackup::Retention::ONE_MONTH:
            return now.addMonths(-1);

        case AutoBackup::Retention::SIX_MONTHS:
            return now.addMonths(-6);

        case AutoBackup::Retention::ONE_YEAR:
            return now.addYears(-1);
    }

    return now.addMonths(-1);
}

//--------------------------------------------------------------------------------------------------
void removeExpired(const QDir& dir, const QDateTime& expiry)
{
    const QString prefix(kFilePrefix);
    const QString suffix(kFileSuffix);
    const QStringList files = dir.entryList({ prefix + '*' + suffix }, QDir::Files);

    for (const QString& file_name : files)
    {
        // The time is taken from the name, because copying the directory changes the times of
        // its files. A file whose name does not carry a time is not ours.
        const QDateTime time = QDateTime::fromString(
            file_name.mid(prefix.size(), file_name.size() - prefix.size() - suffix.size()), kTimeFormat);
        if (!time.isValid() || time >= expiry)
            continue;

        if (!QFile::remove(dir.filePath(file_name)))
            LOG(ERROR) << "Unable to remove expired backup" << file_name;
        else
            LOG(INFO) << "Expired backup removed:" << file_name;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Backup::Result AutoBackup::run(Database& db, const QString& directory, Retention retention)
{
    // A relative path would resolve against whatever the working directory happens to be.
    if (!QDir::isAbsolutePath(directory))
    {
        LOG(ERROR) << "Backup directory is not an absolute path:" << directory;
        return Backup::Result::FILE_ERROR;
    }

    QDir dir(directory);
    if (!dir.mkpath("."))
    {
        LOG(ERROR) << "Unable to create backup directory" << directory;
        return Backup::Result::FILE_ERROR;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QString file_name = kFilePrefix + now.toString(kTimeFormat) + kFileSuffix;

    const Backup::Result result = Backup::exportToFile(db, dir.filePath(file_name));
    if (result != Backup::Result::SUCCESS)
        return result;

    LOG(INFO) << "Backup written:" << dir.filePath(file_name);
    removeExpired(dir, expiryTime(retention, now));
    return result;
}
