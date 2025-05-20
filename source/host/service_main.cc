//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/service_main.h"

#include "base/application.h"
#include "base/logging.h"
#include "base/meta_types.h"
#include "base/sys_info.h"
#include "base/threading/asio_event_dispatcher.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/host_storage.h"
#include "host/service.h"
#include "host/service_constants.h"
#include "proto/meta_types.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/service_controller.h"
#include "base/win/session_info.h"
#include "base/win/session_enumerator.h"
#endif // defined(Q_OS_WINDOWS)

#include <QCommandLineParser>
#include <QProcessEnvironment>
#include <QSysInfo>

namespace host {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    base::ServiceController controller =
        base::ServiceController::open(host::kHostServiceName);
    if (!controller.isValid())
    {
        out << "Failed to access the service. Not enough rights or service not installed."
            << Qt::endl;
        return 1;
    }

    if (!controller.start())
    {
        out << "Failed to start the service." << Qt::endl;
        return 1;
    }

    out << "The service started successfully." << Qt::endl;
    return 0;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
int stopService(QTextStream& out)
{
    base::ServiceController controller = base::ServiceController::open(host::kHostServiceName);
    if (!controller.isValid())
    {
        out << "Failed to access the service. Not enough rights or service not installed."
            << Qt::endl;
        return 1;
    }

    if (!controller.stop())
    {
        out << "Failed to stop the service." << Qt::endl;
        return 1;
    }

    out << "The service has stopped successfully." << Qt::endl;
    return 0;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
int installService(QTextStream& out)
{
    base::ServiceController controller = base::ServiceController::install(
        host::kHostServiceName, host::kHostServiceDisplayName, base::Application::applicationFilePath());
    if (!controller.isValid())
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller.setDescription(host::kHostServiceDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (base::ServiceController::isRunning(host::kHostServiceName))
        stopService(out);

    if (!base::ServiceController::remove(host::kHostServiceName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
std::optional<QString> currentSessionName()
{
#if defined(Q_OS_WINDOWS)
    DWORD process_session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &process_session_id))
        return std::nullopt;

    DWORD console_session_id = WTSGetActiveConsoleSessionId();
    if (console_session_id == process_session_id)
        return QString();

    base::SessionInfo current_session_info(process_session_id);
    if (!current_session_info.isValid())
        return std::nullopt;

    QString user_name = current_session_info.userName().toLower();
    QString domain = current_session_info.domain().toLower();

    if (user_name.isEmpty())
        return std::nullopt;

    using TimeInfo = std::pair<base::SessionId, qint64>;
    using TimeInfoList = std::vector<TimeInfo>;

    // Enumarate all user sessions.
    TimeInfoList times;
    for (base::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (user_name != it.userName().toLower())
            continue;

        base::SessionInfo session_info(it.sessionId());
        if (!session_info.isValid())
            continue;

        // In Windows Server can have multiple RDP sessions with the same username. We get a list of
        // sessions with the same username and session connection time.
        times.emplace_back(session_info.sessionId(), session_info.connectTime());
    }

    // Sort the list by the time it was connected to the server.
    std::sort(times.begin(), times.end(), [](const TimeInfo& p1, const TimeInfo& p2)
    {
        return p1.second < p2.second;
    });

    // We are looking for the current session in the sorted list.
    size_t user_number = 0;
    for (size_t i = 0; i < times.size(); ++i)
    {
        if (times[i].first == current_session_info.sessionId())
        {
            // Save the user number in the list.
            user_number = i;
            break;
        }
    }

    // The session name contains the username and domain to reliably distinguish between local and
    // domain users. It also contains the user number found above. This way, users will receive the
    // same ID based on the time they were connected to the server.
    return user_name + "@" + domain + "@" + QString::number(user_number);
#else
    return std::nullopt;
#endif
}

//--------------------------------------------------------------------------------------------------
void printDebugInfo()
{
    LOG(LS_INFO) << "Command line: " << base::Application::arguments();
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING
                 << " (arch: " << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

    LOG(LS_INFO) << "Computer name: '" << base::SysInfo::computerName() << "'";
    LOG(LS_INFO) << "Environment variables: "
                 << QProcessEnvironment::systemEnvironment().toStringList();

#if defined(Q_OS_WINDOWS)
    MEMORYSTATUSEX memory_status;
    memset(&memory_status, 0, sizeof(memory_status));
    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status))
    {
        static const quint32 kMB = 1024 * 1024;

        LOG(LS_INFO) << "Total physical memory: " << (memory_status.ullTotalPhys / kMB)
                     << "MB (free: " << (memory_status.ullAvailPhys / kMB) << "MB)";
        LOG(LS_INFO) << "Total page file: " << (memory_status.ullTotalPageFile / kMB)
                     << "MB (free: " << (memory_status.ullAvailPageFile / kMB) << "MB)";
        LOG(LS_INFO) << "Total virtual memory: " << (memory_status.ullTotalVirtual / kMB)
                     << "MB (free: " << (memory_status.ullAvailVirtual / kMB) << "MB)";
    }
    else
    {
        PLOG(LS_ERROR) << "GlobalMemoryStatusEx failed";
    }

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
    }
    else
    {
        base::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_ERROR) << "Unable to get session info";
        }
        else
        {
            LOG(LS_INFO) << "Process session ID: " << session_id;
            LOG(LS_INFO) << "Running in user session: '" << session_info.userName() << "'";
            LOG(LS_INFO) << "Session connect state: "
                         << base::SessionInfo::connectStateToString(session_info.connectState());
            LOG(LS_INFO) << "WinStation name: '" << session_info.winStationName() << "'";
            LOG(LS_INFO) << "Domain name: '" << session_info.domain() << "'";
        }
    }

    wchar_t username[64] = { 0 };
    DWORD username_size = sizeof(username) / sizeof(username[0]);
    if (!GetUserNameW(username, &username_size))
    {
        PLOG(LS_ERROR) << "GetUserNameW failed";
    }

    LOG(LS_INFO) << "Running as user: '" << username << "'";
    LOG(LS_INFO) << "Active console session ID: " << WTSGetActiveConsoleSessionId();

    LOG(LS_INFO) << "Active sessions";
    LOG(LS_INFO) << "#####################################################";
    for (base::SessionEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        LOG(LS_INFO) << enumerator.sessionName() << " (id=" << enumerator.sessionId()
        << " host='" << enumerator.hostName() << "', user='" << enumerator.userName()
        << "', domain='" << enumerator.domainName() << "', locked="
        << enumerator.isUserLocked() << ")";
    }
    LOG(LS_INFO) << "#####################################################";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
int hostServiceMain(int& argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    LOG(LS_INFO) << "Integrity check passed successfully";

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

    if (!integrityCheck())
    {
        LOG(LS_ERROR) << "Integrity check failed. Application stopped";
        return 1;
    }

    printDebugInfo();

    QCommandLineParser parser;

#if defined(Q_OS_WINDOWS)
    QCommandLineOption install_option("install",
        base::Application::translate("ServiceMain", "Install service."));
    QCommandLineOption remove_option("remove",
        base::Application::translate("ServiceMain", "Remove service."));
    QCommandLineOption start_option("start",
        base::Application::translate("ServiceMain", "Start service."));
    QCommandLineOption stop_option("stop",
        base::Application::translate("ServiceMain", "Stop service."));

    parser.addOption(install_option);
    parser.addOption(remove_option);
    parser.addOption(start_option);
    parser.addOption(stop_option);
#endif // defined(Q_OS_WINDOWS)

    QCommandLineOption hostid_option("host-id",
        base::Application::translate("ServiceMain", "Get current host id."));

    parser.addOption(hostid_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    QTextStream out(stdout, QIODevice::WriteOnly);

    if (parser.isSet(hostid_option))
    {
        std::optional<QString> session_name = currentSessionName();
        if (!session_name.has_value())
            return 1;

        HostStorage storage;
        out << storage.lastHostId(*session_name) << Qt::endl;
        return 0;
    }
#if defined(Q_OS_WINDOWS)
    else if (parser.isSet(install_option))
    {
        return installService(out);
    }
    else if (parser.isSet(remove_option))
    {
        return removeService(out);
    }
    else if (parser.isSet(start_option))
    {
        return startService(out);
    }
    else if (parser.isSet(stop_option))
    {
        return stopService(out);
    }
#endif // defined(Q_OS_WINDOWS)
    else
    {
        base::registerMetaTypes();
        proto::registerMetaTypes();

        return Service().exec(application);
    }
}

} // namespace host
