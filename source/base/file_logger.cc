//
// PROJECT:         Aspia
// FILE:            base/file_logger.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/file_logger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QThread>

namespace aspia {

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

} // namespace aspia
