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

#include "common/update_installer.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/files/file_util.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include "base/process_util.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <QProcess>
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

namespace {

const qint64 kHashBlockSize = 1024 * 1024;

//--------------------------------------------------------------------------------------------------
bool hasExpectedHash(const QString& file_path, const QString& expected)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open file:" << file.errorString();
        return false;
    }

    GenericHash hash(GenericHash::SHA256);

    while (!file.atEnd())
    {
        QByteArray block = file.read(kHashBlockSize);
        if (block.isEmpty())
        {
            LOG(ERROR) << "Unable to read file:" << file.errorString();
            return false;
        }

        hash.addData(block);
    }

    QString actual = QString::fromLatin1(hash.result().toHex());
    if (actual != expected)
    {
        LOG(ERROR) << "Hash of the downloaded file is" << actual << "and not" << expected;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void removeFile(const QString& file_path)
{
    if (!QFile::remove(file_path))
        LOG(ERROR) << "Unable to remove file:" << file_path;
}

//--------------------------------------------------------------------------------------------------
// Creates a directory nobody but the owner can write to. The package waits there between the check
// of its hash and the start of the installer of the system, and a directory a user can write to
// would let the file be replaced in between. An empty string is returned on failure.
QString createPrivateDirectory()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };
    if (!GetWindowsDirectoryW(buffer, MAX_PATH))
    {
        PLOG(ERROR) << "GetWindowsDirectoryW failed";
        return QString();
    }

    // The name is random because the temp directory of the system lists its contents to everyone
    // and lets everyone create entries in it.
    QString path = QString::fromWCharArray(buffer) + "/Temp/aspia_update_" +
        QString::fromLatin1(Random::byteArray(16).toHex());

    if (!QDir().mkdir(path))
    {
        LOG(ERROR) << "Unable to create directory:" << path;
        return QString();
    }

    return path;
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QByteArray name = "/var/tmp/aspia_update_XXXXXX";
    if (!mkdtemp(name.data()))
    {
        PLOG(ERROR) << "mkdtemp failed";
        return QString();
    }

    QString path = QString::fromLocal8Bit(name);

    // The package itself is public, and the account the package manager reads local files as has
    // to reach it. Only writing has to stay with the owner.
    if (chmod(name.constData(), 0755) != 0)
    {
        PLOG(ERROR) << "Unable to set permissions:" << path;
        QDir().rmdir(path);
        return QString();
    }

    return path;
#else
    return QString();
#endif
}

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateInstaller::UpdateInstaller(Mode mode, QObject* parent)
    : QObject(parent),
      mode_(mode)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
UpdateInstaller::~UpdateInstaller()
{
    LOG(TRACE) << "Dtor";
    cleanup();
}

//--------------------------------------------------------------------------------------------------
// static
bool UpdateInstaller::isSupported(const QString& format)
{
#if defined(Q_OS_WINDOWS)
    return format == "msi";
#elif defined(Q_OS_LINUX)
    // The package is handed to the manager that knows its format, and only that one.
    if (format == "deb")
        return !QStandardPaths::findExecutable("apt-get").isEmpty();

    if (format == "rpm")
        return !QStandardPaths::findExecutable("dnf").isEmpty();

    return false;
#elif defined(Q_OS_MACOS)
    return format == "pkg";
#else
    Q_UNUSED(format);
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
QString UpdateInstaller::createPackageFile(const UpdateInfo& update_info)
{
    cleanup();

    QString directory = createPrivateDirectory();
    if (directory.isEmpty())
        return QString();

    // The package manager of the system is given a file it recognizes by its name. The name is
    // random for the same reason the name of the directory is.
    QString file_path = QDir::toNativeSeparators(
        directory + "/" + QString::fromLatin1(Random::byteArray(16).toHex()) + "." +
        update_info.format());

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(ERROR) << "Unable to create file:" << file.errorString();
        QDir().rmdir(directory);
        return QString();
    }

    file.close();

    update_info_ = update_info;
    directory_ = directory;
    file_path_ = file_path;

    return file_path;
}

//--------------------------------------------------------------------------------------------------
UpdateInstaller::Result UpdateInstaller::install()
{
    if (file_path_.isEmpty())
    {
        LOG(ERROR) << "No package to install";
        return Result::FAILED;
    }

    if (!hasExpectedHash(file_path_, update_info_.sha256()))
        return Result::DAMAGED;

    if (!startInstaller())
        return Result::FAILED;

    return Result::STARTED;
}

//--------------------------------------------------------------------------------------------------
void UpdateInstaller::cleanup()
{
    if (!file_path_.isEmpty())
    {
        removeFile(file_path_);
        file_path_.clear();
    }

    if (!directory_.isEmpty())
    {
        if (!QDir().rmdir(directory_))
            LOG(ERROR) << "Unable to remove directory:" << directory_;

        directory_.clear();
    }
}

//--------------------------------------------------------------------------------------------------
bool UpdateInstaller::startInstaller()
{
#if defined(Q_OS_WINDOWS)
    // Normal install. A person watching gets a basic UI with no modal dialog boxes, the service
    // installs with no UI at all.
    QString arguments = "/i \"" + file_path_ + "\"" + (mode_ == Mode::USER ? " /qb-!" : " /qn");

    if (!ProcessUtil::createProcess("msiexec", arguments, ProcessUtil::ExecuteMode::ELEVATE))
    {
        LOG(ERROR) << "Unable to start msiexec process (cmd:" << arguments << ")";
        return false;
    }

    LOG(INFO) << "msiexec is started (cmd:" << arguments << ")";

    // The package is msiexec's from here on, and it is still holding it when this process ends.
    if (!removeAtNextStart(file_path_))
        PLOG(ERROR) << "Unable to remove file at the next start:" << file_path_;

    if (!removeAtNextStart(directory_))
        PLOG(ERROR) << "Unable to remove directory at the next start:" << directory_;

    file_path_.clear();
    directory_.clear();

    // msiexec replaces the files of the running application, so there is nothing left to wait for.
    // The result is reported once the caller has control back.
    QTimer::singleShot(MilliSeconds(0), this, [this]()
    {
        emit sig_finished(true, QString());
    });

    return true;
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    QStringList arguments;

#if defined(Q_OS_MACOS)
    // The installer of the system puts the package where the package itself says it goes.
    arguments << "installer" << "-pkg" << file_path_ << "-target" << "/";
#else
    // Both managers install a local file and pull in what it depends on.
    if (update_info_.format() == "deb")
        arguments << "apt-get" << "install" << "-y" << file_path_;
    else if (update_info_.format() == "rpm")
        arguments << "dnf" << "install" << "-y" << file_path_;
    else
    {
        LOG(ERROR) << "No package manager for format:" << update_info_.format();
        return false;
    }
#endif // defined(Q_OS_MACOS)

    if (mode_ == Mode::SERVICE)
    {
#if defined(Q_OS_LINUX)
        // The package restarts the service, and systemd kills the whole control group of a unit
        // it stops, so the installer dies with it. A transient unit of its own outlives that and
        // the install finishes even though it is the updated service that started it.
        if (!QStandardPaths::findExecutable("systemd-run").isEmpty())
        {
            arguments = QStringList() << "systemd-run" << "--collect" << "--quiet"
                                      << "--unit=aspia-host-update" << arguments;
        }
#endif // defined(Q_OS_LINUX)

        QString program = arguments.takeFirst();

        if (!QProcess::startDetached(program, arguments))
        {
            LOG(ERROR) << "Unable to create update process (cmd:" << program << arguments << ")";
            return false;
        }

        LOG(INFO) << "Update process started (cmd:" << program << arguments << ")";

        // The process is detached, so there is nobody left to clean up after it.
        file_path_.clear();
        directory_.clear();

        QTimer::singleShot(MilliSeconds(0), this, [this]()
        {
            emit sig_finished(true, QString());
        });

        return true;
    }

    QProcess* process = new QProcess(this);

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process](int exit_code, QProcess::ExitStatus exit_status)
    {
        LOG(INFO) << "Installer finished with exit code:" << exit_code
                  << "(status:" << exit_status << ")";

        // The package managers of Linux report the failure on the standard error, the installer
        // of macOS on the standard output, where its last line is the one that matters.
        QString error = QString::fromLocal8Bit(process->readAllStandardError()).trimmed();
        if (error.isEmpty())
        {
            error = QString::fromLocal8Bit(process->readAllStandardOutput())
                .trimmed().section('\n', -1);
        }

        process->deleteLater();
        cleanup();

        emit sig_finished(exit_code == 0, error);
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error)
    {
        if (error != QProcess::FailedToStart)
            return;

        QString error_string = process->errorString();
        LOG(ERROR) << "Unable to start installer:" << error_string;

        process->deleteLater();
        cleanup();

        emit sig_finished(false, error_string);
    });

    QString program = arguments.takeFirst();

    process->start(program, arguments);
    process->closeWriteChannel();

    return true;
#else
    return false;
#endif
}
