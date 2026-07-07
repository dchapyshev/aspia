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

#include "host/task_manager.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>

#include <WtsApi32.h>
#elif defined(Q_OS_LINUX)
#include <QDBusConnection>
#include <QDBusMessage>
#endif

#include "base/logging.h"
#include "base/service_controller.h"
#include "base/session_id.h"
#include "base/sys_info.h"
#include "host/process_monitor.h"
#include "proto/task_manager.h"

//--------------------------------------------------------------------------------------------------
TaskManager::TaskManager(QObject* parent)
    : QObject(parent),
      process_monitor_(ProcessMonitor::create())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TaskManager::~TaskManager()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void TaskManager::readMessage(const proto::task_manager::ClientToHost& message)
{
    if (message.has_process_list_request())
    {
        sendProcessList(message.process_list_request().flags());
    }
    else if (message.has_end_process_request())
    {
        if (process_monitor_)
        {
            process_monitor_->endProcess(
                static_cast<ProcessMonitor::ProcessId>(message.end_process_request().pid()));
        }
    }
    else if (message.has_service_list_request())
    {
        sendServiceList();
    }
    else if (message.has_service_request())
    {
        if (message.service_request().name().empty())
        {
            LOG(ERROR) << "Service name not specified";
            return;
        }

        switch (message.service_request().command())
        {
            case proto::task_manager::ServiceRequest::COMMAND_START:
            {
                std::unique_ptr<ServiceController> controller = ServiceController::open(
                    QString::fromStdString(message.service_request().name()));
                if (!controller)
                {
                    LOG(ERROR) << "Unable to open service:" << message.service_request().name();
                    return;
                }

                if (!controller->start())
                {
                    LOG(ERROR) << "Unable to start service:" << message.service_request().name();
                    return;
                }

                sendServiceList();
            }
            break;

            case proto::task_manager::ServiceRequest::COMMAND_STOP:
            {
                std::unique_ptr<ServiceController> controller = ServiceController::open(
                    QString::fromStdString(message.service_request().name()));
                if (!controller)
                {
                    LOG(ERROR) << "Unable to open service:" << message.service_request().name();
                    return;
                }

                if (!controller->stop())
                {
                    LOG(ERROR) << "Unable to stop service:" << message.service_request().name();
                    return;
                }

                sendServiceList();
            }
            break;

            default:
            {
                LOG(ERROR) << "Unknown command for service request:"
                           << message.service_request().command();
            }
            break;
        }
    }
    else if (message.has_user_list_request())
    {
        sendUserList();
    }
    else if (message.has_user_request())
    {
        if (message.user_request().session_id() == kInvalidSessionId)
        {
            LOG(ERROR) << "Invalid session id";
            return;
        }

        SessionId session_id = message.user_request().session_id();

        switch (message.user_request().command())
        {
            case proto::task_manager::UserRequest::COMMAND_DISCONNECT:
            {
#if defined(Q_OS_WINDOWS)
                if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, session_id, FALSE))
                {
                    PLOG(ERROR) << "WTSDisconnectSession failed";
                    return;
                }
#else
                LOG(ERROR) << "Disconnecting a session is not supported on this platform";
                return;
#endif
            }
            break;

            case proto::task_manager::UserRequest::COMMAND_LOGOFF:
            {
#if defined(Q_OS_WINDOWS)
                if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, session_id, FALSE))
                {
                    PLOG(ERROR) << "WTSLogoffSession failed";
                    return;
                }
#elif defined(Q_OS_LINUX)
                QDBusMessage call = QDBusMessage::createMethodCall(
                    "org.freedesktop.login1", "/org/freedesktop/login1",
                    "org.freedesktop.login1.Manager", "TerminateSession");
                call << QString::number(static_cast<qint64>(session_id));

                const QDBusMessage reply = QDBusConnection::systemBus().call(call);
                if (reply.type() == QDBusMessage::ErrorMessage)
                {
                    LOG(ERROR) << "TerminateSession failed:" << reply.errorMessage();
                    return;
                }
#endif
            }
            break;

            default:
            {
                LOG(ERROR) << "Unknown command for user request:" << message.user_request().command();
            }
            break;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled task manager request";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendProcessList(quint32 flags)
{
    if (!process_monitor_)
    {
        LOG(ERROR) << "Process monitor is not available on this platform";
        return;
    }

    proto::task_manager::HostToClient message;

    bool reset_cache = false;
    if (flags & proto::task_manager::ProcessListRequest::RESET_CACHE)
        reset_cache = true;

    proto::task_manager::ProcessList* process_list = message.mutable_process_list();
    process_list->set_cpu_usage(process_monitor_->calcCpuUsage());
    process_list->set_memory_usage(process_monitor_->calcMemoryUsage());

    const ProcessMonitor::ProcessMap& processes = process_monitor_->processes(reset_cache);
    for (auto it = processes.cbegin(), it_end = processes.cend(); it != it_end; ++it)
    {
        proto::task_manager::Process* item = process_list->add_process();

        ProcessMonitor::ProcessId process_id = it.key();
        const ProcessMonitor::ProcessEntry& process_info = it.value();

        if (process_info.process_name_changed)
            item->set_process_name(process_info.process_name.toStdString());

        if (process_info.user_name_changed)
            item->set_user_name(process_info.user_name.toStdString());

        if (process_info.file_path_changed)
            item->set_file_path(process_info.file_path.toStdString());

        item->set_process_id(process_id);
        item->set_session_id(process_info.session_id);
        item->set_cpu_usage(process_info.cpu_ratio);
        item->set_mem_private_working_set(process_info.mem_private_working_set);
        item->set_mem_working_set(process_info.mem_working_set);
        item->set_mem_peak_working_set(process_info.mem_peak_working_set);
        item->set_mem_working_set_delta(process_info.mem_working_set_delta);
        item->set_thread_count(process_info.thread_count);
    }

    emit sig_taskManagerMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendServiceList()
{
    proto::task_manager::HostToClient message;
    proto::task_manager::ServiceList* service_list = message.mutable_service_list();

    const QList<SysInfo::Service> services = SysInfo::services();
    for (const SysInfo::Service& service : services)
    {
        proto::task_manager::Service* item = service_list->add_service();

        item->set_name(service.name.toStdString());
        item->set_display_name(service.display_name.toStdString());
        item->set_description(service.description.toStdString());

        switch (service.startup_type)
        {
            case SysInfo::Service::StartupType::AUTO_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_AUTO_START);
                break;

            case SysInfo::Service::StartupType::DEMAND_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_DEMAND_START);
                break;

            case SysInfo::Service::StartupType::DISABLED:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_DISABLED);
                break;

            case SysInfo::Service::StartupType::BOOT_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_BOOT_START);
                break;

            case SysInfo::Service::StartupType::SYSTEM_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_UNKNOWN);
                break;
        }

        switch (service.status)
        {
            case SysInfo::Service::Status::CONTINUE_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_CONTINUE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSE_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_PAUSE_PENDING);
                break;

            case SysInfo::Service::Status::PAUSED:
                item->set_status(proto::task_manager::Service::STATUS_PAUSED);
                break;

            case SysInfo::Service::Status::RUNNING:
                item->set_status(proto::task_manager::Service::STATUS_RUNNING);
                break;

            case SysInfo::Service::Status::START_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_START_PENDING);
                break;

            case SysInfo::Service::Status::STOP_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_STOP_PENDING);
                break;

            case SysInfo::Service::Status::STOPPED:
                item->set_status(proto::task_manager::Service::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::task_manager::Service::STATUS_UNKNOWN);
                break;
        }
    }

    emit sig_taskManagerMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendUserList()
{
    proto::task_manager::HostToClient message;
    proto::task_manager::UserList* user_list = message.mutable_user_list();

    const QList<SysInfo::Session> sessions = SysInfo::sessions();
    for (const SysInfo::Session& session : sessions)
    {
        proto::task_manager::User* item = user_list->add_user();

        item->set_user_name(session.user_name.toStdString());
        item->set_session_id(session.id);
        item->set_session_name(session.session_name.toStdString());
        item->set_client_name(session.client_name.toStdString());

        switch (session.connect_state)
        {
            case SysInfo::Session::ConnectState::ACTIVE:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_ACTIVE);
                break;

            case SysInfo::Session::ConnectState::CONNECTED:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_CONNECTED);
                break;

            case SysInfo::Session::ConnectState::CONNECT_QUERY:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_CONNECT_QUERY);
                break;

            case SysInfo::Session::ConnectState::SHADOW:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_SHADOW);
                break;

            case SysInfo::Session::ConnectState::DISCONNECTED:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_DISCONNECTED);
                break;

            case SysInfo::Session::ConnectState::IDLE:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_IDLE);
                break;

            case SysInfo::Session::ConnectState::LISTEN:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_LISTEN);
                break;

            case SysInfo::Session::ConnectState::RESET:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_RESET);
                break;

            case SysInfo::Session::ConnectState::DOWN:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_DOWN);
                break;

            case SysInfo::Session::ConnectState::INIT:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_INIT);
                break;

            default:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_UNKNOWN);
                break;
        }
    }

    emit sig_taskManagerMessage(message);
}
