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

#include "base/core_application.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/service_controller.h"
#include "base/session_id.h"
#include "base/system_error.h"
#include "base/time_types.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/threading/asio_event_dispatcher.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"
#include "host/host_constants.h"
#include "host/host_utils.h"
#include "host/win/portable_package.h"
#include "host/workers/audio_worker.h"
#include "host/workers/input_worker.h"
#include "host/workers/portable_desktop_worker.h"
#include "host/workers/portable_user_worker.h"
#include "host/workers/screen_worker.h"
#include "version.h"

namespace {

const char kLaunchOption[] = "--portable";
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
    // The copy made by the launcher lives in the directory named by the random ID of its run.
    if (PortableHost::isStartedByLauncher())
        return QDir(BasePaths::currentAppDir()).dirName();

    // In user mode the host runs from the executable the user started, one instance per executable
    // and session.
    const QByteArray path = QFileInfo(BasePaths::currentApp()).canonicalFilePath().toLower().toUtf8();
    const QByteArray hash = GenericHash::hash(GenericHash::BLAKE2s256, path).toHex().left(16);

    return QString::fromLatin1(hash) + '-' + QString::number(currentProcessSessionId());
}

//--------------------------------------------------------------------------------------------------
bool startedByOurProcess()
{
    const quint32 parent_pid = ProcessUtil::parentProcessId(ProcessUtil::currentProcessId());
    if (parent_pid == 0)
        return false;

    const QString parent_path = QFileInfo(ProcessUtil::filePath(parent_pid)).canonicalFilePath();
    if (parent_path.isEmpty())
        return false;

    return parent_path.compare(
        QFileInfo(BasePaths::currentApp()).canonicalFilePath(), Qt::CaseInsensitive) == 0;
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

    // Start the GUI in this launcher's session.
    if (!ProcessUtil::createProcess(QDir::toNativeSeparators(file_path), QString()))
        LOG(ERROR) << "Unable to start the portable GUI";

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

//--------------------------------------------------------------------------------------------------
// Runs the portable host in user mode. The service worker lives in this process without a Windows
// service, and the GUI runs in the user's session. Used when the user declined the elevation prompt.
int launchUserMode(int& argc, char* argv[])
{
    ScopedHandle mutex(CreateMutexW(nullptr, FALSE, qUtf16Printable(mutexName(runId()))));
    if (!mutex.isValid())
    {
        PLOG(ERROR) << "CreateMutexW failed";
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LOG(INFO) << "Portable host is already running in user mode";

        // The new GUI finds the running one and brings up its window.
        if (!ProcessUtil::createProcess(QDir::toNativeSeparators(BasePaths::currentApp()), QString()))
            LOG(ERROR) << "Unable to start the portable GUI";
        return 0;
    }

    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);

    CoreApplication application(argc, argv);
    application.addWorker(std::make_unique<PortableUserWorker>());
    application.addWorker(std::make_unique<PortableDesktopWorker>());
    application.addWorker(std::make_unique<ScreenWorker>());
    application.addWorker(std::make_unique<InputWorker>());
    application.addWorker(std::make_unique<AudioWorker>());

    PortableUserWorker* user_worker = CoreApplication::findWorker<PortableUserWorker>();
    PortableDesktopWorker* desktop_worker = CoreApplication::findWorker<PortableDesktopWorker>();
    ScreenWorker* screen_worker = CoreApplication::findWorker<ScreenWorker>();
    InputWorker* input_worker = CoreApplication::findWorker<InputWorker>();
    AudioWorker* audio_worker = CoreApplication::findWorker<AudioWorker>();

    QObject::connect(user_worker, &PortableUserWorker::sig_desktopClientStarted,
                     desktop_worker, &PortableDesktopWorker::onClientStarted, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_desktopClientMessage,
                     desktop_worker, &PortableDesktopWorker::onClientMessage, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_desktopClientFinished,
                     desktop_worker, &PortableDesktopWorker::onClientFinished, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_desktopClientChannelChanged,
                     desktop_worker, &PortableDesktopWorker::onClientChannelChanged, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_clientMessage,
                     user_worker, &PortableUserWorker::onDesktopClientMessage, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_pauseChanged,
                     desktop_worker, &PortableDesktopWorker::onUserPause, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_lockMouseChanged,
                     desktop_worker, &PortableDesktopWorker::onUserLockMouse, Qt::QueuedConnection);
    QObject::connect(user_worker, &PortableUserWorker::sig_lockKeyboardChanged,
                     desktop_worker, &PortableDesktopWorker::onUserLockKeyboard, Qt::QueuedConnection);

    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_selectScreen,
                     screen_worker, &ScreenWorker::onSelectScreen, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_keyFrameRequested,
                     screen_worker, &ScreenWorker::onKeyFrameRequested, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_preferredSizeChanged,
                     screen_worker, &ScreenWorker::onSetPreferredSize, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_configure,
                     screen_worker, &ScreenWorker::onConfigure, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_overflowStateChanged,
                     screen_worker, &ScreenWorker::onOverflowStateChanged, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_bandwidthChanged,
                     screen_worker, &ScreenWorker::onBandwidthChanged, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_stopCapture,
                     screen_worker, &ScreenWorker::onStopCapture, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_paused,
                     screen_worker, &ScreenWorker::onSetPaused, Qt::QueuedConnection);

    QObject::connect(screen_worker, &ScreenWorker::sig_videoData,
                     desktop_worker, &PortableDesktopWorker::onVideoData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_cursorShapeData,
                     desktop_worker, &PortableDesktopWorker::onCursorShapeData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_cursorPositionData,
                     desktop_worker, &PortableDesktopWorker::onCursorPositionData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_screenListData,
                     desktop_worker, &PortableDesktopWorker::onScreenListData, Qt::QueuedConnection);
    QObject::connect(screen_worker, &ScreenWorker::sig_screenTypeData,
                     desktop_worker, &PortableDesktopWorker::onScreenTypeData, Qt::QueuedConnection);

    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_injectKeyEvent,
                     input_worker, &InputWorker::onInjectKeyEvent, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_injectTextEvent,
                     input_worker, &InputWorker::onInjectTextEvent, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_injectMouseEvent,
                     input_worker, &InputWorker::onInjectMouseEvent, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_injectTouchEvent,
                     input_worker, &InputWorker::onInjectTouchEvent, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_paused,
                     input_worker, &InputWorker::onSetPaused, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_mouseLocked,
                     input_worker, &InputWorker::onSetMouseLocked, Qt::QueuedConnection);
    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_keyboardLocked,
                     input_worker, &InputWorker::onSetKeyboardLocked, Qt::QueuedConnection);

    QObject::connect(desktop_worker, &PortableDesktopWorker::sig_audioEnabled,
                     audio_worker, &AudioWorker::onSetEnabled, Qt::QueuedConnection);
    QObject::connect(audio_worker, &AudioWorker::sig_audioData,
                     desktop_worker, &PortableDesktopWorker::onAudioData, Qt::QueuedConnection);

    // Start the GUI in this launcher's session.
    if (!ProcessUtil::createProcess(QDir::toNativeSeparators(BasePaths::currentApp()), QString()))
        LOG(ERROR) << "Unable to start the portable GUI";

    LOG(INFO) << "Portable host is started in user mode";
    return application.exec();
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
bool PortableHost::isLauncherInvocation(int argc, char* argv[])
{
    // The internal relaunch (asking for elevation) is always the launcher.
    if (argc == 2 && qstrcmp(argv[1], kLaunchOption) == 0)
        return true;

    // A role invocation (the --agent process and the like) is never the launcher.
    if (argc != 1)
        return false;

    // A bare invocation is the launcher when the user started it, and the host window when our own
    // launcher did. The launcher started it when it is the copy in the run directory (elevated) or
    // when its parent is our own binary (user mode).
    return !isStartedByLauncher() && !startedByOurProcess();
}

//--------------------------------------------------------------------------------------------------
// static
int PortableHost::runLauncher(int argc, char* argv[])
{
    // The user mode host runs in this process and captures the screen, so DPI awareness is set here,
    // before anything (the elevation request included) could create a window.
    HostUtils::setDpiAwareness();

    const bool relaunched = argc == 2 && qstrcmp(argv[1], kLaunchOption) == 0;
    if (argc > 1 && !relaunched)
    {
        LOG(ERROR) << "Unsupported command line of the portable host";
        return 1;
    }

    if (ProcessUtil::isProcessElevated())
        return launch();

    if (relaunched)
    {
        LOG(ERROR) << "The launcher is not elevated";
        return 1;
    }

    // Ask for elevation (the temporary service needs administrator rights). If the user declines the
    // UAC prompt, fall back to running without it, in user mode.
    if (ProcessUtil::createProcess(QDir::toNativeSeparators(BasePaths::currentApp()),
            QString::fromLatin1(kLaunchOption), ProcessUtil::ExecuteMode::ELEVATE))
    {
        return 0;
    }

    LOG(INFO) << "Elevation was declined; starting the portable host in user mode";
    return launchUserMode(argc, argv);
}

//--------------------------------------------------------------------------------------------------
// static
bool PortableHost::isActive()
{
    static const bool active = PortablePackage::builtInSettings().has_value() &&
        (isStartedByLauncher() || !ProcessUtil::isProcessElevated());
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
