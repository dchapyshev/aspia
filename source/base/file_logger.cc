//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/file_logger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QThread>

namespace aspia {

#if 0
QScopedPointer<QFile> FileLogger::file_;

namespace {

const char* typeName(QtMsgType type)
{
    switch (type)
    {
        case QtInfoMsg:
            return "INFO";

        case QtDebugMsg:
            return "DEBUG";

        case QtWarningMsg:
            return "WARNING";

        case QtCriticalMsg:
            return "CRITICAL";

        case QtFatalMsg:
            return "FATAL";

        default:
            return "UNKNOWN";
    }
}

} // namespace

FileLogger::FileLogger() = default;

FileLogger::~FileLogger()
{
    qInfo("Logging finished");
}

bool FileLogger::startLogging(const QString& prefix)
{
    QDir directory(QDir::tempPath() + QLatin1String("/aspia"));
    if (!directory.exists())
    {
        if (!directory.mkpath(directory.path()))
        {
            qWarning() << "Unable to create logging directory: " << directory.path();
            return false;
        }
    }

    QString file_path = QString("%1/%2_%3.log")
        .arg(directory.path())
        .arg(prefix)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh.mm.ss.zzz")));

    file_.reset(new QFile(file_path));

    if (!file_->open(QFile::WriteOnly | QFile::Append | QFile::Text))
    {
        qWarning() << "Unable to create logging file: " << file_->errorString();
        return false;
    }

    qInstallMessageHandler(messageHandler);

    qInfo() << "Logging started.";
    return true;
}

// static
void FileLogger::messageHandler(QtMsgType type,
                                const QMessageLogContext& context,
                                const QString& msg)
{
    QString filename(context.file);

    int last_slash_pos = filename.lastIndexOf(QLatin1Char('\\'));
    if (last_slash_pos == -1)
        last_slash_pos = filename.lastIndexOf(QLatin1Char('/'));

    if (last_slash_pos != -1)
        filename.remove(0, last_slash_pos + 1);

    QTextStream stream(file_.data());

    stream << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))
           << QLatin1Char(' ')
           << typeName(type)
           << QLatin1Char(' ')
           << filename << QLatin1Char(':') << context.line << QLatin1String("] ")
           << msg
           << endl;

    stream.flush();
}

#endif

} // namespace aspia
