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

#include "host/task_manager.h"

#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/win/service_enumerator.h"
#include "base/win/service_controller.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "host/process_monitor.h"

namespace host {

//--------------------------------------------------------------------------------------------------
TaskManager::TaskManager(Delegate* delegate)
    : process_monitor_(std::make_unique<ProcessMonitor>()),
      delegate_(delegate)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(delegate_);
}

//--------------------------------------------------------------------------------------------------
TaskManager::~TaskManager()
{
    LOG(LS_INFO) << "Dtor";
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
            LOG(LS_ERROR) << "Service name not specified";
            return;
        }

        switch (message.service_request().command())
        {
            case proto::task_manager::ServiceRequest::COMMAND_START:
            {
                base::win::ServiceController controller = base::win::ServiceController::open(
                    base::utf16FromUtf8(message.service_request().name()));
                if (!controller.isValid())
                {
                    LOG(LS_ERROR) << "Unable to open service: " << message.service_request().name();
                    return;
                }

                if (!controller.start())
                {
                    LOG(LS_ERROR) << "Unable to start service: " << message.service_request().name();
                    return;
                }

                sendServiceList();
            }
            break;

            case proto::task_manager::ServiceRequest::COMMAND_STOP:
            {
                base::win::ServiceController controller = base::win::ServiceController::open(
                    base::utf16FromUtf8(message.service_request().name()));
                if (!controller.isValid())
                {
                    LOG(LS_ERROR) << "Unable to open service: " << message.service_request().name();
                    return;
                }

                if (!controller.stop())
                {
                    LOG(LS_ERROR) << "Unable to stop service: " << message.service_request().name();
                    return;
                }

                sendServiceList();
            }
            break;

            default:
            {
                LOG(LS_ERROR) << "Unknown command for service request: "
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
        if (message.user_request().session_id() == base::kInvalidSessionId)
        {
            LOG(LS_ERROR) << "Invalid session id";
            return;
        }

        switch (message.user_request().command())
        {
            case proto::task_manager::UserRequest::COMMAND_DISCONNECT:
            {
                if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, message.user_request().session_id(), FALSE))
                {
                    PLOG(LS_ERROR) << "WTSLogoffSession failed";
                    return;
                }
            }
            break;

            case proto::task_manager::UserRequest::COMMAND_LOGOFF:
            {
                if (!WTSLogoffSession(WTS_CURRENT_SERVER_HANDLE, message.user_request().session_id(), FALSE))
                {
                    PLOG(LS_ERROR) << "WTSLogoffSession failed";
                    return;
                }
            }
            break;

            default:
            {
                LOG(LS_ERROR) << "Unknown command for user request: "
                              << message.user_request().command();
            }
            break;
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled task manager request";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendProcessList(uint32_t flags)
{
    proto::task_manager::HostToClient message;

    bool reset_cache = false;
    if (flags & proto::task_manager::ProcessListRequest::RESET_CACHE)
        reset_cache = true;

    proto::task_manager::ProcessList* process_list = message.mutable_process_list();
    process_list->set_cpu_usage(process_monitor_->calcCpuUsage());
    process_list->set_memory_usage(process_monitor_->calcMemoryUsage());

    const ProcessMonitor::ProcessMap& processes = process_monitor_->processes(reset_cache);
    for (const auto& process : processes)
    {
        proto::task_manager::Process* item = process_list->add_process();

        ProcessMonitor::ProcessId process_id = process.first;
        const ProcessMonitor::ProcessEntry& process_info = process.second;

        if (process_info.process_name_changed)
            item->set_process_name(process_info.process_name);

        if (process_info.user_name_changed)
            item->set_user_name(process_info.user_name);

        if (process_info.file_path_changed)
            item->set_file_path(process_info.file_path);

        item->set_session_id(process_info.session_id);
        item->set_process_id(process_id);
        item->set_session_id(process_info.session_id);
        item->set_cpu_usage(process_info.cpu_ratio);
        item->set_mem_private_working_set(process_info.mem_private_working_set);
        item->set_mem_working_set(process_info.mem_working_set);
        item->set_mem_peak_working_set(process_info.mem_peak_working_set);
        item->set_mem_working_set_delta(process_info.mem_working_set_delta);
        item->set_thread_count(process_info.thread_count);
    }

    delegate_->onTaskManagerMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendServiceList()
{
    proto::task_manager::HostToClient message;
    proto::task_manager::ServiceList* service_list = message.mutable_service_list();

    for (base::win::ServiceEnumerator enumerator(base::win::ServiceEnumerator::Type::SERVICES);
         !enumerator.isAtEnd(); enumerator.advance())
    {
        proto::task_manager::Service* item = service_list->add_service();

        item->set_name(enumerator.name());
        item->set_display_name(enumerator.displayName());
        item->set_description(enumerator.description());

        switch (enumerator.startupType())
        {
            case base::win::ServiceEnumerator::StartupType::AUTO_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_AUTO_START);
                break;

            case base::win::ServiceEnumerator::StartupType::DEMAND_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_DEMAND_START);
                break;

            case base::win::ServiceEnumerator::StartupType::DISABLED:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_DISABLED);
                break;

            case base::win::ServiceEnumerator::StartupType::BOOT_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_BOOT_START);
                break;

            case base::win::ServiceEnumerator::StartupType::SYSTEM_START:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_SYSTEM_START);
                break;

            default:
                item->set_startup_type(proto::task_manager::Service::STARTUP_TYPE_UNKNOWN);
                break;
        }

        switch (enumerator.status())
        {
            case base::win::ServiceEnumerator::Status::CONTINUE_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_CONTINUE_PENDING);
                break;

            case base::win::ServiceEnumerator::Status::PAUSE_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_PAUSE_PENDING);
                break;

            case base::win::ServiceEnumerator::Status::PAUSED:
                item->set_status(proto::task_manager::Service::STATUS_PAUSED);
                break;

            case base::win::ServiceEnumerator::Status::RUNNING:
                item->set_status(proto::task_manager::Service::STATUS_RUNNING);
                break;

            case base::win::ServiceEnumerator::Status::START_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_START_PENDING);
                break;

            case base::win::ServiceEnumerator::Status::STOP_PENDING:
                item->set_status(proto::task_manager::Service::STATUS_STOP_PENDING);
                break;

            case base::win::ServiceEnumerator::Status::STOPPED:
                item->set_status(proto::task_manager::Service::STATUS_STOPPED);
                break;

            default:
                item->set_status(proto::task_manager::Service::STATUS_UNKNOWN);
                break;
        }
    }

    delegate_->onTaskManagerMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManager::sendUserList()
{
    proto::task_manager::HostToClient message;
    proto::task_manager::UserList* user_list = message.mutable_user_list();

    for (base::win::SessionEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        // Skip services.
        if (enumerator.sessionId() == 0)
            continue;

        base::win::SessionInfo session_info(enumerator.sessionId());
        if (!session_info.isValid())
            continue;

        proto::task_manager::User* item = user_list->add_user();

        item->set_user_name(enumerator.userName());
        item->set_session_id(enumerator.sessionId());
        item->set_session_name(enumerator.sessionName());
        item->set_client_name(session_info.clientName().toStdString());

        switch (session_info.connectState())
        {
            case base::win::SessionInfo::ConnectState::ACTIVE:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_ACTIVE);
                break;

            case base::win::SessionInfo::ConnectState::CONNECTED:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_CONNECTED);
                break;

            case base::win::SessionInfo::ConnectState::CONNECT_QUERY:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_CONNECT_QUERY);
                break;

            case base::win::SessionInfo::ConnectState::SHADOW:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_SHADOW);
                break;

            case base::win::SessionInfo::ConnectState::DISCONNECTED:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_DISCONNECTED);
                break;

            case base::win::SessionInfo::ConnectState::IDLE:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_IDLE);
                break;

            case base::win::SessionInfo::ConnectState::LISTEN:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_LISTEN);
                break;

            case base::win::SessionInfo::ConnectState::RESET:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_RESET);
                break;

            case base::win::SessionInfo::ConnectState::DOWN:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_DOWN);
                break;

            case base::win::SessionInfo::ConnectState::INIT:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_INIT);
                break;

            default:
                item->set_connect_state(proto::task_manager::User::CONNECT_STATE_UNKNOWN);
                break;
        }
    }

    delegate_->onTaskManagerMessage(message);
}

} // namespace host
