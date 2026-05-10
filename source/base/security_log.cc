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

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_local.h"

#include <aclapi.h>
#include <sddl.h>

#include <string>
#elif defined(Q_OS_UNIX)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

const qint64 kMaxLogFileAge = 90; // 90 days.
const char kFileMask[] = "*.log";

#if defined(Q_OS_WINDOWS)
// Owner: Local System.
// Group: Local System.
// DACL: protected (no inheritance from parent), allow Local System full access, allow
// BUILTIN\Users read-only access. ACEs are object/container-inherited so files created inside
// the directory pick up the same restriction.
const wchar_t kDirectoryDescriptor[] =
    L"O:SY"
    L"G:SY"
    L"D:PAR(A;OICI;FA;;;SY)(A;OICI;FR;;;BU)";

// File descriptor (no inheritance flags, applied to a specific file).
const wchar_t kFileDescriptor[] =
    L"O:SY"
    L"G:SY"
    L"D:P(A;;FA;;;SY)(A;;FR;;;BU)";
#elif defined(Q_OS_UNIX)
const mode_t kFileMode = 0644;
const mode_t kDirectoryMode = 0755;
#endif

QMutex g_writer_mutex;
QFile g_writer_file;

//--------------------------------------------------------------------------------------------------
QString defaultLogDirectory()
{
#if defined(Q_OS_WINDOWS)
    QString dir = BasePaths::appDataDir();
    if (dir.isEmpty())
        return QString();
    return dir + "/security";
#elif defined(Q_OS_MACOS)
    return "/Library/Logs/aspia/security";
#else
    return "/var/log/aspia/security";
#endif
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool applyDescriptor(const QString& path, const wchar_t* sddl)
{
    ScopedLocal<PSECURITY_DESCRIPTOR> sd;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd.recieve(), nullptr))
    {
        PLOG(ERROR) << "ConvertStringSecurityDescriptorToSecurityDescriptorW failed";
        return false;
    }

    PSID owner = nullptr;
    BOOL owner_defaulted = FALSE;
    if (!GetSecurityDescriptorOwner(sd, &owner, &owner_defaulted))
    {
        PLOG(ERROR) << "GetSecurityDescriptorOwner failed";
        return false;
    }

    PSID group = nullptr;
    BOOL group_defaulted = FALSE;
    if (!GetSecurityDescriptorGroup(sd, &group, &group_defaulted))
    {
        PLOG(ERROR) << "GetSecurityDescriptorGroup failed";
        return false;
    }

    PACL dacl = nullptr;
    BOOL dacl_present = FALSE;
    BOOL dacl_defaulted = FALSE;
    if (!GetSecurityDescriptorDacl(sd, &dacl_present, &dacl, &dacl_defaulted))
    {
        PLOG(ERROR) << "GetSecurityDescriptorDacl failed";
        return false;
    }

    std::wstring wpath = path.toStdWString();

    DWORD result = SetNamedSecurityInfoW(const_cast<LPWSTR>(wpath.c_str()), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
        PROTECTED_DACL_SECURITY_INFORMATION, owner, group, dacl, nullptr);
    if (result != ERROR_SUCCESS)
    {
        LOG(ERROR) << "SetNamedSecurityInfoW failed:" << path << "error:" << result;
        return false;
    }

    return true;
}
#elif defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
bool applyOwnership(const QString& path, mode_t mode)
{
    if (geteuid() != 0)
    {
        LOG(ERROR) << "Process is not running as root, cannot secure security log path:" << path;
        return false;
    }

    QByteArray local = path.toLocal8Bit();

    if (chown(local.constData(), 0, 0) != 0)
    {
        PLOG(ERROR) << "chown failed:" << path;
        return false;
    }

    if (chmod(local.constData(), mode) != 0)
    {
        PLOG(ERROR) << "chmod failed:" << path;
        return false;
    }

    return true;
}
#endif

//--------------------------------------------------------------------------------------------------
bool applyFilePermissions(const QString& path)
{
#if defined(Q_OS_WINDOWS)
    return applyDescriptor(path, kFileDescriptor);
#elif defined(Q_OS_UNIX)
    return applyOwnership(path, kFileMode);
#else
    Q_UNUSED(path)
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
bool applyDirectoryPermissions(const QString& path)
{
#if defined(Q_OS_WINDOWS)
    return applyDescriptor(path, kDirectoryDescriptor);
#elif defined(Q_OS_UNIX)
    return applyOwnership(path, kDirectoryMode);
#else
    Q_UNUSED(path)
    return false;
#endif
}

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

    QString path = defaultLogDirectory();
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

    if (!applyDirectoryPermissions(path))
    {
        LOG(ERROR) << "Failed to apply security log directory permissions:" << path;
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

    if (!applyFilePermissions(file_path))
    {
        LOG(ERROR) << "Failed to apply security log file permissions:" << file_path;
        g_writer_file.close();
        QFile::remove(file_path);
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
