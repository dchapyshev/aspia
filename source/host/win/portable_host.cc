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

#include "host/win/portable_host.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <qt_windows.h>
#include <ShlObj.h>
#include <TlHelp32.h>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/service_controller.h"
#include "base/system_error.h"
#include "base/time_types.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"
#include "host/host_constants.h"
#include "host/win/portable_package.h"

namespace {

const char kLaunchOption[] = "--portable";
const char kGuiHiddenOption[] = "--hidden";
const char kExecutableName[] = "aspia_host.exe";
const char kBaseDirectoryName[] = "AspiaPortable";
const char kDataDirectoryName[] = "data";
const char kServiceNamePrefix[] = "aspia-host-portable-";
const wchar_t kServiceDisplayName[] = L"Aspia Host Portable";

// Only SYSTEM and Administrators have access to the settings of the portable host.
const char kDataDirectorySddl[] = "D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";

const MilliSeconds kPollInterval(100);
const int kServiceStartAttempts = 300;
const int kRemoveAttempts = 30;

//--------------------------------------------------------------------------------------------------
// Where the copies of the host live. It is Program Files: only administrators can create files
// there, so nobody can replace the executable the service runs as SYSTEM.
QString baseDirectory()
{
    ScopedCoMem<wchar_t> buffer;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetKnownFolderPath failed:" << SystemError::toString(hr);
        return QString();
    }

    return QDir::fromNativeSeparators(QString::fromWCharArray(buffer)) + '/' + kBaseDirectoryName;
}

//--------------------------------------------------------------------------------------------------
QString runId()
{
    return QDir(BasePaths::currentAppDir()).dirName();
}

//--------------------------------------------------------------------------------------------------
QString serviceName(const QString& run_id)
{
    return kServiceNamePrefix + run_id;
}

//--------------------------------------------------------------------------------------------------
QString mutexName(const QString& run_id)
{
    return "Global\\aspia-host-portable-" + run_id;
}

//--------------------------------------------------------------------------------------------------
void removeService(const QString& name)
{
    if (!ServiceController::isInstalled(name))
        return;

    if (ServiceController::isRunning(name))
    {
        std::unique_ptr<ServiceController> service = ServiceController::open(name);
        if (service)
            service->stop();
    }

    if (ServiceController::remove(name))
        LOG(INFO) << "Service" << name << "is removed";
    else
        LOG(ERROR) << "Unable to remove service:" << name;
}

//--------------------------------------------------------------------------------------------------
// Terminates the processes still running from |run_dir| so the directory can be removed. The GUI
// outlives the service (like the installed host's) and a desktop agent may still be exiting.
void terminateRunProcesses(const QString& run_dir)
{
    ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot.isValid())
    {
        PLOG(ERROR) << "CreateToolhelp32Snapshot failed";
        return;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry))
        return;

    const QString prefix = QDir::toNativeSeparators(run_dir) + '\\';

    do
    {
        if (QString::fromWCharArray(entry.szExeFile).compare(
                QLatin1String(kExecutableName), Qt::CaseInsensitive) != 0)
        {
            continue;
        }

        const QString path = QDir::toNativeSeparators(ProcessUtil::filePath(entry.th32ProcessID));
        if (!path.startsWith(prefix, Qt::CaseInsensitive))
            continue;

        ScopedHandle process(OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID));
        if (process.isValid() && TerminateProcess(process, 0))
            LOG(INFO) << "Terminated process from the run directory:" << path;
    }
    while (Process32NextW(snapshot, &entry));
}

//--------------------------------------------------------------------------------------------------
void removeDirectory(const QString& path)
{
    for (int i = 0; i < kRemoveAttempts; ++i)
    {
        if (!QFileInfo::exists(path) || QDir(path).removeRecursively())
            return;

        // The processes of the host started from the directory may still be exiting.
        QThread::sleep(Seconds(1));
    }

    LOG(ERROR) << "Unable to remove directory:" << path;
}

//--------------------------------------------------------------------------------------------------
// Removes what is left of the runs whose launcher is gone (killed, or the user logged off).
void removeStaleRuns(const QString& base_dir)
{
    const QStringList run_ids = QDir(base_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& run_id : run_ids)
    {
        ScopedHandle mutex(OpenMutexW(SYNCHRONIZE, FALSE, qUtf16Printable(mutexName(run_id))));
        if (mutex.isValid())
            continue;

        LOG(INFO) << "Removing stale run:" << run_id;
        removeService(serviceName(run_id));
        terminateRunProcesses(base_dir + '/' + run_id);
        removeDirectory(base_dir + '/' + run_id);
    }
}

//--------------------------------------------------------------------------------------------------
bool createDataDirectory(const QString& path)
{
    ScopedSd security_descriptor = convertSddlToSd(QString::fromLatin1(kDataDirectorySddl));
    if (!security_descriptor)
        return false;

    SECURITY_ATTRIBUTES attributes;
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = security_descriptor.get();
    attributes.bInheritHandle = FALSE;

    if (!CreateDirectoryW(qUtf16Printable(QDir::toNativeSeparators(path)), &attributes))
    {
        PLOG(ERROR) << "CreateDirectoryW failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// Copies the executable to |run_dir|, registers the temporary service for it and waits until the
// service stops.
bool runHost(const QString& run_dir, const QString& run_id)
{
    if (!QDir().mkpath(run_dir) || !createDataDirectory(run_dir + '/' + kDataDirectoryName))
    {
        LOG(ERROR) << "Unable to create directory:" << run_dir;
        return false;
    }

    const QString file_path = run_dir + '/' + kExecutableName;
    if (!QFile::copy(BasePaths::currentApp(), file_path))
    {
        LOG(ERROR) << "Unable to copy the executable to:" << file_path;
        return false;
    }

    ScopedScHandle manager(
        OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return false;
    }

    const QString command_line = '"' + QDir::toNativeSeparators(file_path) + "\" --service";

    ScopedScHandle service(CreateServiceW(manager, qUtf16Printable(serviceName(run_id)),
        kServiceDisplayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE, qUtf16Printable(command_line), nullptr, nullptr, nullptr, nullptr,
        nullptr));
    if (!service.isValid())
    {
        PLOG(ERROR) << "CreateServiceW failed";
        return false;
    }

    if (!StartServiceW(service, 0, nullptr))
    {
        PLOG(ERROR) << "StartServiceW failed";
        return false;
    }

    SERVICE_STATUS_PROCESS status;
    memset(&status, 0, sizeof(status));

    for (int i = 0; i < kServiceStartAttempts; ++i)
    {
        DWORD bytes_needed = 0;
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(&status),
                                  sizeof(status), &bytes_needed))
        {
            PLOG(ERROR) << "QueryServiceStatusEx failed";
            return false;
        }

        if (status.dwCurrentState != SERVICE_START_PENDING)
            break;

        QThread::sleep(kPollInterval);
    }

    if (status.dwCurrentState != SERVICE_RUNNING)
    {
        LOG(ERROR) << "Service is not started (state:" << status.dwCurrentState << ")";
        return false;
    }

    ScopedHandle process(OpenProcess(SYNCHRONIZE, FALSE, status.dwProcessId));
    if (!process.isValid())
    {
        PLOG(ERROR) << "OpenProcess failed";
        return false;
    }

    // Start the GUI in this launcher's session, the session of the user who started the portable host.
    // The service never launches or relaunches the GUI; the GUI stays in this session and when the
    // user closes it the service sees the IPC channel drop and stops. The GUI retries the IPC
    // connection, so the brief moment before the service IPC server is ready does not matter.
    if (!ProcessUtil::createProcess(
            QDir::toNativeSeparators(file_path), QString::fromLatin1(kGuiHiddenOption)))
    {
        LOG(ERROR) << "Unable to start the portable GUI";
    }

    LOG(INFO) << "Portable host is started from:" << run_dir;
    WaitForSingleObject(process, INFINITE);
    LOG(INFO) << "Portable host is stopped";
    return true;
}

//--------------------------------------------------------------------------------------------------
// Picks a free run directory under |base_dir| and holds its marker for as long as the run lives.
bool reserveRun(const QString& base_dir, QString* run_id_out, QString* run_dir_out, ScopedHandle* mutex_out)
{
    QString run_id;
    QString run_dir;

    do
    {
        run_id = QString::fromLatin1(Random::byteArray(8).toHex());
        run_dir = base_dir + '/' + run_id;
    }
    while (QFileInfo::exists(run_dir));

    ScopedHandle mutex(CreateMutexW(nullptr, FALSE, qUtf16Printable(mutexName(run_id))));
    if (!mutex.isValid())
    {
        PLOG(ERROR) << "CreateMutexW failed";
        return false;
    }

    *run_id_out = run_id;
    *run_dir_out = run_dir;
    mutex_out->reset(mutex.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
int launch()
{
    const QString base_dir = baseDirectory();
    if (base_dir.isEmpty())
        return 1;

    removeStaleRuns(base_dir);

    QString run_id;
    QString run_dir;
    ScopedHandle mutex;
    if (!reserveRun(base_dir, &run_id, &run_dir, &mutex))
        return 1;

    const bool result = runHost(run_dir, run_id);

    removeService(serviceName(run_id));
    terminateRunProcesses(run_dir);
    removeDirectory(run_dir);

    // Removed only when there are no other runs.
    QDir().rmdir(base_dir);

    return result ? 0 : 1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool PortableHost::isStartedByLauncher()
{
    const QFileInfo file_info(BasePaths::currentApp());
    if (file_info.fileName().compare(kExecutableName, Qt::CaseInsensitive) != 0)
        return false;

    const QString base = QFileInfo(file_info.absolutePath()).absolutePath();
    return base.compare(baseDirectory(), Qt::CaseInsensitive) == 0;
}

//--------------------------------------------------------------------------------------------------
// static
int PortableHost::runLauncher(int argc, char* argv[])
{
    const bool relaunched = argc == 2 && qstrcmp(argv[1], kLaunchOption) == 0;
    if (argc > 1 && !relaunched)
    {
        LOG(ERROR) << "Unsupported command line of the portable host";
        return 1;
    }

    if (ProcessUtil::isProcessElevated())
        return launch();

    // The temporary service needs administrator rights.
    if (relaunched)
    {
        LOG(ERROR) << "The launcher is not elevated";
        return 1;
    }

    return ProcessUtil::createProcess(
        QDir::toNativeSeparators(BasePaths::currentApp()),
        QString::fromLatin1(kLaunchOption),
        ProcessUtil::ExecuteMode::ELEVATE) ? 0 : 1;
}

//--------------------------------------------------------------------------------------------------
// static
bool PortableHost::isActive()
{
    static const bool active = PortablePackage::builtInSettings().has_value() && isStartedByLauncher();
    return active;
}

//--------------------------------------------------------------------------------------------------
// static
QString PortableHost::serviceName()
{
    return ::serviceName(runId());
}

//--------------------------------------------------------------------------------------------------
// static
QString PortableHost::desktopAgentChannelId()
{
    return QString::fromLatin1(kDesktopAgentChannelId) + '-' + runId();
}

//--------------------------------------------------------------------------------------------------
// static
QString PortableHost::uiChannelId()
{
    return QString::fromLatin1(kHostUiChannelId) + '-' + runId();
}
