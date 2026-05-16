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

#include "base/security_log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include "base/logging.h"
#include "base/files/base_paths.h"

namespace {

const qint64 kMaxLogFileAge = 180; // 180 days.
const char kFileMask[] = "*.log";

QMutex g_writer_mutex;
QFile g_writer_file;

//--------------------------------------------------------------------------------------------------
void removeOldFiles(const QString& dir)
{
    QDateTime threshold = QDateTime::currentDateTime().addDays(-kMaxLogFileAge);

    QDir current_dir(dir);
    QFileInfoList files = current_dir.entryInfoList(
        QStringList() << kFileMask, QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for (const auto& file : std::as_const(files))
    {
        if (file.lastModified() < threshold)
            QFile::remove(file.filePath());
    }
}

//--------------------------------------------------------------------------------------------------
QString ensureLogDirectory()
{
    static QString dir_path;

    if (!dir_path.isEmpty())
        return dir_path;

    QString path = securityLogDirectory();
    if (path.isEmpty())
    {
        LOG(ERROR) << "Security log directory is empty";
        return QString();
    }

    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(path))
    {
        LOG(ERROR) << "Failed to create security log directory:" << path;
        return QString();
    }

    removeOldFiles(path);
    dir_path = path;

    LOG(INFO) << "Security log directory ready:" << dir_path;
    return dir_path;
}

//--------------------------------------------------------------------------------------------------
const char* eventName(SecurityEvent event)
{
    switch (event)
    {
        case SEC_CONNECT:    return "CONNECT";
        case SEC_DISCONNECT: return "DISCONNECT";
        case SEC_ERROR:      return "ERROR";
        case SEC_WARNING:    return "WARNING";
        case SEC_INFO:       return "INFO";
        default:             return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
void openFileIfNeeded()
{
    QString dir_path = ensureLogDirectory();
    if (dir_path.isEmpty())
        return;

    // Files rotate weekly. The tag is the ISO 8601 week (year + week number), so every host
    // writing within the same calendar week ends up in the same file.
    int week_year = 0;
    int week_number = QDate::currentDate().weekNumber(&week_year);

    QString file_path = QString("%1/%2-W%3.log")
        .arg(dir_path).arg(week_year).arg(week_number, 2, 10, QChar('0'));

    if (g_writer_file.isOpen() && g_writer_file.fileName() != file_path)
        g_writer_file.close();

    if (g_writer_file.isOpen())
        return;

    g_writer_file.setFileName(file_path);
    if (!g_writer_file.open(QFile::WriteOnly | QFile::Append | QFile::Text))
    {
        LOG(ERROR) << "Failed to open security log file:" << file_path
                   << "error:" << g_writer_file.errorString();
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void writeLine(const QString& line)
{
    QMutexLocker locker(&g_writer_mutex);

    openFileIfNeeded();
    if (!g_writer_file.isOpen())
        return;

    g_writer_file.write(line.toUtf8());
    g_writer_file.flush();
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString securityLogDirectory()
{
#if defined(Q_OS_WINDOWS)
    QString dir = BasePaths::appConfigDir();
    if (dir.isEmpty())
        return QString();
    return dir + "/secure/logs";
#elif defined(Q_OS_MACOS)
    return "/Library/Logs/aspia/security";
#else
    return "/var/log/aspia/security";
#endif
}

//--------------------------------------------------------------------------------------------------
SecurityLogMessage::SecurityLogMessage(SecurityEvent event)
    : event_(event),
      stream_(&string_)
{
    stream_ << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << eventName(event_);
}

//--------------------------------------------------------------------------------------------------
SecurityLogMessage::~SecurityLogMessage()
{
    stream_ << Qt::endl;
    writeLine(string_);
}
