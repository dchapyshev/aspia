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

#include "host/desktop_client.h"

#include <QTimer>

#include "base/application.h"
#include "base/logging.h"
#include "base/power_controller.h"
#include "base/numeric_utils.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_video.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/safe_mode_util.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "host/host_storage.h"
#include "host/service.h"
#include "host/win/system_info.h"
#include "host/win/task_manager.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopClient::DesktopClient(base::TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      dettach_time_(QTime::currentTime()),
      fake_capture_timer_(new QTimer(this)),
      overflow_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &DesktopClient::sendSessionList, Qt::QueuedConnection);
#endif // defined(Q_OS_WINDOWS)

    connect(fake_capture_timer_, &QTimer::timeout, this, [this]()
    {
        if (dettach_time_.secsTo(QTime::currentTime()) > 15)
        {
            CLOG(WARNING) << "Timeout when desktop client starting";
            emit sig_finished();
            return;
        }

        proto::video::HostToClient message;
        proto::video::Packet* packet = message.mutable_packet();
        packet->set_error_code(proto::video::ERROR_CODE_TEMPORARY);
        send(proto::desktop::CHANNEL_ID_VIDEO, base::serialize(message));
    });

    fake_capture_timer_->setInterval(std::chrono::milliseconds(30));
    fake_capture_timer_->start();

    connect(overflow_timer_, &QTimer::timeout, this, &DesktopClient::onOverflowCheck);
    overflow_timer_->setInterval(std::chrono::milliseconds(1000));

    if (!qEnvironmentVariableIsSet("ASPIA_NO_OVERFLOW_DETECTION"))
    {
        CLOG(INFO) << "Overflow detection enabled";
        overflow_timer_->start();
    }
}

//--------------------------------------------------------------------------------------------------
DesktopClient::~DesktopClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool DesktopClient::isAttached() const
{
    return ipc_channel_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
QString DesktopClient::attach()
{
    CCHECK(!ipc_channel_);
    CCHECK(!ipc_server_);

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &DesktopClient::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred, this, &DesktopClient::onIpcErrorOccurred);

    QString channel_name = base::IpcServer::createUniqueId();

    if (!ipc_server_->start(channel_name))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        emit sig_finished();
        return QString();
    }

    return channel_name;
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::dettach()
{
    if (ipc_channel_)
    {
        ipc_channel_->disconnect(); // Disconnect all signals.
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    dettach_time_ = QTime::currentTime();
    fake_capture_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onUserMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD || channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
            return;

        if (!(config_.flags() & proto::control::ENABLE_CLIPBOARD))
            return;
    }

    send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onStart()
{
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    if (net_channel_id == proto::desktop::CHANNEL_ID_CONTROL)
    {
        proto::control::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse service message";
            return;
        }

        if (message.has_config())
        {
            config_ = message.config();

            proto::desktop::ServiceToAgentClient message;
            message.mutable_config()->CopyFrom(config_);
            sendIpcServiceMessage(base::serialize(message));
        }
        else if (message.has_session_list_request())
        {
            sendSessionList();
        }
        else if (message.has_switch_session())
        {
            CLOG(INFO) << "Received:" << message.switch_session();

            base::SessionId session_id = message.switch_session().session_id();
            if (session_id == base::kInvalidSessionId || session_id == base::kServiceSessionId)
            {
                CLOG(ERROR) << "Invalid session id:" << session_id;
                return;
            }

            emit sig_switchSession(session_id);
        }
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD)
    {
        if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
            return;

        if (!(config_.flags() & proto::control::ENABLE_CLIPBOARD))
            return;

        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_USER)
    {
        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
            return;

        if (!(config_.flags() & proto::control::ENABLE_CLIPBOARD))
            return;

        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_POWER)
    {
        proto::power::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse power message";
            return;
        }

        readPowerControl(message.power_control());
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_SYSTEM_INFO)
    {
        proto::system_info::SystemInfoRequest request;
        if (!base::parse(buffer, &request))
        {
            CLOG(ERROR) << "Unable to parse system info message";
            return;
        }

        readSystemInfo(request);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_TASK_MANAGER)
    {
        proto::task_manager::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse task manager message";
            return;
        }

        readTaskManager(message);
    }
    else
    {
        quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id);
        if (ipc_channel_)
            ipc_channel_->send(channel_id, buffer);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onBandwidthChanged(qint64 bandwidth)
{
    proto::desktop::ServiceToAgentClient message;
    proto::desktop::BandwidthChange* bandwidth_change = message.mutable_bandwidth_change();
    bandwidth_change->set_bandwidth(bandwidth);
    sendIpcServiceMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcNewConnection()
{
    CCHECK(ipc_server_);
    CCHECK(!ipc_channel_);

    if (!ipc_server_->hasPendingConnections())
    {
        CLOG(ERROR) << "No pending IPC connections";
        emit sig_finished();
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    ipc_server_->disconnect();
    ipc_server_->deleteLater();
    ipc_server_ = nullptr;

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopClient::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopClient::onIpcMessageReceived);

    proto::peer::SessionType session_type = sessionType();
    CCHECK(session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE ||
           session_type == proto::peer::SESSION_TYPE_DESKTOP_VIEW);

    proto::desktop::ServiceToAgentClient message;
    proto::desktop::Description* description = message.mutable_description();

    description->set_session_type(session_type);
    *description->mutable_version() = base::serialize(version());
    description->set_os_name(osName().toStdString());
    description->set_computer_name(computerName().toStdString());
    description->set_display_name(displayName().toStdString());
    description->set_architecture(architecture().toStdString());
    description->set_user_name(userName().toStdString());

    sendIpcServiceMessage(base::serialize(message));

    fake_capture_timer_->stop();
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcErrorOccurred()
{
    CLOG(ERROR) << "Error in IPC server";
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable)
{
    quint16 net_channel_id = base::lowWord(channel_id);
    quint16 ipc_channel_id = base::highWord(channel_id);

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
    {
        send(net_channel_id, buffer, reliable);
    }
    else
    {
        // Nothing.
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcDisconnected()
{
    if (!ipc_channel_)
        return;

    CLOG(INFO) << "IPC channel disconnected";
    dettach();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onOverflowCheck()
{
    static const qint64 kCriticalPendingBytes = 1 * 1024 * 1024; // 1 MB
    static const qint64 kWarningPendingBytes = 512 * 1024; // 512 kB

    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;
    qint64 pending = pendingBytes();

    if (pending > kCriticalPendingBytes)
        state = proto::desktop::Overflow::STATE_CRITICAL;
    else if (pending > kWarningPendingBytes)
        state = proto::desktop::Overflow::STATE_WARNING;

    if (state != last_state_)
    {
        CLOG(INFO) << "Overflow state:" << state << "pending:" << pending;
        last_state_ = state;
    }

    proto::desktop::ServiceToAgentClient message;
    proto::desktop::Overflow* overflow = message.mutable_overflow();
    overflow->set_state(state);
    sendIpcServiceMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTaskManagerMessage(const proto::task_manager::HostToClient& message)
{
    send(proto::desktop::CHANNEL_ID_TASK_MANAGER, base::serialize(message), true);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendIpcServiceMessage(const QByteArray& buffer)
{
    quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SERVICE, 0);
    if (ipc_channel_)
        ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendSessionList()
{
#if defined(Q_OS_WINDOWS)
    proto::control::HostToClient message;
    proto::control::SessionList* session_list = message.mutable_session_list();

    base::SessionId console_session_id = base::activeConsoleSessionId();
    base::SessionId current_session_id = 0;
    if (ipc_channel_)
        current_session_id = ipc_channel_->sessionId();

    session_list->set_current_session_id(current_session_id);
    session_list->set_console_session_id(console_session_id);

    for (base::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (it.sessionId() == base::kServiceSessionId) // Don't add system session.
            continue;

        base::SessionInfo session_info(it.sessionId());
        if (!session_info.isValid())
            continue;

        proto::control::Session* session = session_list->add_session();
        session->set_session_id(session_info.sessionId());
        session->set_user_name(session_info.userName().toStdString());
        session->set_domain_name(session_info.domain().toStdString());
        session->set_is_locked(session_info.isUserLocked());
        session->set_is_active(session_info.connectState() == base::SessionInfo::ConnectState::ACTIVE);
    }

    CLOG(INFO) << "Send:" << *session_list;
    send(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message));
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readPowerControl(const proto::power::Control& control)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    CLOG(INFO) << "Received:" << control;

    switch (control.action())
    {
        case proto::power::Control::ACTION_SHUTDOWN:
            base::PowerController::shutdown();
            break;

        case proto::power::Control::ACTION_REBOOT:
            base::PowerController::reboot();
            break;

        case proto::power::Control::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(Q_OS_WINDOWS)
            if (!base::SafeModeUtil::setSafeModeService(Service::kName, true))
            {
                CLOG(ERROR) << "Failed to add service to start in safe mode";
                return;
            }

            CLOG(INFO) << "Service added successfully to start in safe mode";

            HostStorage storage;
            storage.setBootToSafeMode(true);

            if (!base::SafeModeUtil::setSafeMode(true))
            {
                CLOG(ERROR) << "Failed to enable boot in Safe Mode";
                return;
            }

            CLOG(INFO) << "Safe Mode boot enabled successfully";
            if (!base::PowerController::reboot())
                CLOG(ERROR) << "Unable to reboot";
#endif // defined(Q_OS_WINDOWS)
        }
        break;

        case proto::power::Control::ACTION_LOGOFF:
        case proto::power::Control::ACTION_LOCK:
        {
            proto::power::ClientToHost message;
            message.mutable_power_control()->CopyFrom(control);

            quint32 channel_id = base::makeUint32(
                proto::desktop::IPC_CHANNEL_ID_SESSION, proto::desktop::CHANNEL_ID_POWER);

            if (ipc_channel_)
                ipc_channel_->send(channel_id, base::serialize(message));
        }
        break;

        default:
            CLOG(ERROR) << "Unhandled power control action:" << control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readSystemInfo(const proto::system_info::SystemInfoRequest& request)
{
#if defined(Q_OS_WINDOWS)
    proto::system_info::SystemInfo reply;
    createSystemInfo(request, &reply);
    send(proto::desktop::CHANNEL_ID_SYSTEM_INFO, base::serialize(reply), true);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readTaskManager(const proto::task_manager::ClientToHost& message)
{
#if defined(Q_OS_WINDOWS)
    if (!task_manager_)
    {
        task_manager_ = new TaskManager(this);
        connect(task_manager_, &TaskManager::sig_taskManagerMessage,
                this, &DesktopClient::onTaskManagerMessage);
    }
    task_manager_->readMessage(message);
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
