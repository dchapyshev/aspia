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

#include <QFileInfo>
#include <QTimer>

#include "base/core_application.h"
#include "base/logging.h"
#include "base/power_controller.h"
#include "base/numeric_utils.h"
#include "base/process_util.h"
#include "base/serialization.h"
#include "base/session_id.h"
#include "base/sys_info.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_manager.h"
#include "host/task_manager.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_power.h"
#include "proto/desktop_video.h"
#include "proto/task_manager.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/safe_mode_util.h"
#include "host/host_storage.h"
#include "host/service.h"
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
DesktopClient::DesktopClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent),
      dettach_time_(QTime::currentTime()),
      fake_capture_timer_(new QTimer(this)),
      overflow_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";

#if defined(Q_OS_WINDOWS)
    connect(CoreApplication::instance(), &CoreApplication::sig_sessionEvent,
            this, &DesktopClient::sendSessionList, Qt::QueuedConnection);
#endif // defined(Q_OS_WINDOWS)

    connect(fake_capture_timer_, &QTimer::timeout, this, [this]()
    {
        if (dettach_time_.secsTo(QTime::currentTime()) > 15)
        {
            CLOG(WARNING) << "Timeout when desktop client starting";
            finish();
            return;
        }

        proto::video::HostToClient message;
        proto::video::Packet* packet = message.mutable_packet();
        packet->set_error_code(proto::video::ERROR_CODE_TEMPORARY);
        send(proto::desktop::CHANNEL_ID_VIDEO, serialize(message));
    });

    fake_capture_timer_->setInterval(std::chrono::milliseconds(30));
    fake_capture_timer_->start();

    // Once the client is finished it must not tick its timers again: the object lives until the deferred
    // delete runs, and a live timer could keep doing work (or call finish() again, which is now a no-op
    // but still wasteful) for a client already removed from the service list.
    connect(this, &Client::sig_finished, this, [this]()
    {
        fake_capture_timer_->stop();
        overflow_timer_->stop();
    });

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

    ipc_server_ = new IpcServer(this);

    connect(ipc_server_, &IpcServer::sig_newConnection, this, &DesktopClient::onIpcNewConnection);
    connect(ipc_server_, &IpcServer::sig_errorOccurred, this, &DesktopClient::onIpcErrorOccurred);

    QString channel_name = IpcServer::createUniqueId();

#if defined(Q_OS_MACOS)
    // The desktop agent is a launchd agent loaded into the active GUI session; in a user's Aqua session
    // it runs as that user, so the data channel must be reachable by non-root. The peer is verified by
    // executable path in onIpcNewConnection().
    const IpcServer::AccessMode access_mode = IpcServer::AccessMode::INTERACTIVE_USER;
#else
    // The desktop agent runs as SYSTEM/root (launched by the service), so the data channel is
    // restricted to system processes.
    const IpcServer::AccessMode access_mode = IpcServer::AccessMode::SYSTEM_ONLY;
#endif // defined(Q_OS_MACOS)

    if (!ipc_server_->start(channel_name, access_mode))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        finish();
        return QString();
    }

    return channel_name;
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::dettach()
{
    // The server may still be waiting for the agent to connect if the desktop manager re-attaches
    // before the previous agent ever showed up; drop it so the CCHECK in attach() does not fire.
    if (ipc_server_)
    {
        ipc_server_->disconnect(); // Disconnect all signals.
        ipc_server_.reset();
    }

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(); // Disconnect all signals.
        ipc_channel_.reset();
    }

    dettach_time_ = QTime::currentTime();
    fake_capture_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onUserMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD || channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (!config_.has_value() || !config_->clipboard())
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
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse service message";
            return;
        }

        if (message.has_config())
        {
            config_ = message.config();
            sendIpcSessionMessage(net_channel_id, buffer);
        }
        else if (message.has_capabilities())
        {
            capabilities_ = message.capabilities();
            sendIpcSessionMessage(net_channel_id, buffer);
        }
        else if (message.has_sessions_request())
        {
            sendSessionList();
        }
        else if (message.has_switch_session())
        {
            CLOG(INFO) << "Received:" << message.switch_session();

            SessionId session_id = message.switch_session().session_id();
            if (session_id == kInvalidSessionId || session_id == kServiceSessionId)
            {
                CLOG(ERROR) << "Invalid session id:" << session_id;
                return;
            }

            emit sig_switchSession(session_id);
        }
        else if (message.has_feedback())
        {
            readFeedback(message.feedback());
        }
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_CLIPBOARD)
    {
        if (!config_.has_value() || !config_->clipboard())
            return;

        // The host GUI handles the system clipboard for graphical sessions; the agent handles it for the
        // VT terminal. Route to both - only the active one acts on it.
        emit sig_userMessage(net_channel_id, buffer);
        sendIpcSessionMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_USER)
    {
        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_FILE)
    {
        if (!config_.has_value() || !config_->clipboard())
            return;

        emit sig_userMessage(net_channel_id, buffer);
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_POWER)
    {
        proto::power::ClientToHost message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse power message";
            return;
        }

        readPowerControl(message.power_control());
    }
    else if (net_channel_id == proto::desktop::CHANNEL_ID_TASK_MANAGER)
    {
        proto::task_manager::ClientToHost message;
        if (!parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse task manager message";
            return;
        }

        readTaskManager(message);
    }
    else
    {
        quint32 channel_id = makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id);
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
    sendIpcServiceMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcNewConnection()
{
    CCHECK(ipc_server_);
    CCHECK(!ipc_channel_);

    if (!ipc_server_->hasPendingConnections())
    {
        CLOG(ERROR) << "No pending IPC connections";
        finish();
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    // Verify the connecting peer's executable is exactly the agent binary we shipped (this very
    // aspia_host binary, run with a "--agent" switch).
    const quint32 client_pid = ipc_channel_->processId();
    const QString expected_path =
        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString actual_path = QFileInfo(ProcessUtil::filePath(client_pid)).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        CLOG(ERROR) << "IPC client has unexpected executable (pid:" << client_pid
                    << "path:" << actual_path << "expected:" << expected_path << ")";
        ipc_channel_.reset();
        finish();
        return;
    }

#if defined(Q_OS_WINDOWS)
    // desktop_agent is spawned by DesktopManager which lives in the same process as us, so the
    // agent's parent PID is our PID. On UNIX the agent is launched via sh/sudo chain plus '&'
    // backgrounding, so its parent PID is not us; the check would reject legitimate agents.
    if (ProcessUtil::parentProcessId(client_pid) != ProcessUtil::currentProcessId())
    {
        CLOG(ERROR) << "IPC client is not our child (pid:" << client_pid << ")";
        ipc_channel_.reset();
        finish();
        return;
    }
#endif

    ipc_server_->disconnect();
    ipc_server_.reset();

    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &DesktopClient::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &DesktopClient::onIpcMessageReceived);

    if (capabilities_.has_value())
    {
        proto::control::ClientToHost message;
        message.mutable_capabilities()->CopyFrom(*capabilities_);
        sendIpcSessionMessage(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
    }

    fake_capture_timer_->stop();
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcErrorOccurred()
{
    CLOG(ERROR) << "Error in IPC server";
    finish();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool reliable)
{
    quint16 net_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (force_reliable_)
        reliable = true;

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
        send(net_channel_id, buffer, reliable);
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
    sendIpcServiceMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTaskManagerMessage(const proto::task_manager::HostToClient& message)
{
    send(proto::desktop::CHANNEL_ID_TASK_MANAGER, serialize(message), true);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendIpcSessionMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    quint32 channel_id = makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id);

    if (ipc_channel_)
        ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendIpcServiceMessage(const QByteArray& buffer)
{
    quint32 channel_id = makeUint32(proto::desktop::IPC_CHANNEL_ID_SERVICE, 0);
    if (ipc_channel_)
        ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendSessionList()
{
#if defined(Q_OS_WINDOWS)
    proto::control::HostToClient message;
    proto::control::SessionList* session_list = message.mutable_session_list();

    SessionId console_session_id = activeConsoleSessionId();
    SessionId current_session_id = 0;
    if (ipc_channel_)
        current_session_id = ipc_channel_->sessionId();

    session_list->set_current_session_id(current_session_id);
    session_list->set_console_session_id(console_session_id);

    const QList<SysInfo::Session> sessions = SysInfo::sessions();
    for (const SysInfo::Session& session_info : sessions)
    {
        proto::control::Session* session = session_list->add_session();
        session->set_session_id(session_info.id);
        session->set_user_name(session_info.user_name.toStdString());
        session->set_domain_name(session_info.domain_name.toStdString());
        session->set_is_locked(session_info.locked);
        session->set_is_active(session_info.connect_state == SysInfo::Session::ConnectState::ACTIVE);
    }

    CLOG(INFO) << "Send:" << *session_list;
    send(proto::desktop::CHANNEL_ID_CONTROL, serialize(message));
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readFeedback(const proto::control::Feedback& feedback)
{
    if (feedback.command_name() == "reliable")
    {
        if (feedback.value_case() != proto::control::Feedback::kBoolean)
        {
            CLOG(WARNING) << "Feedback 'reliable' expects boolean value";
            return;
        }

        if (force_reliable_ != feedback.boolean())
        {
            CLOG(INFO) << "Force reliable changed:" << force_reliable_ << "->" << feedback.boolean();
            force_reliable_ = feedback.boolean();
        }
    }
    else
    {
        CLOG(WARNING) << "Unknown feedback command:" << feedback.command_name();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readPowerControl(const proto::power::Control& control)
{
    CLOG(INFO) << "Received:" << control;
    switch (control.action())
    {
        case proto::power::Control::ACTION_SHUTDOWN:
            PowerController::shutdown();
            break;

        case proto::power::Control::ACTION_REBOOT:
            PowerController::reboot();
            break;

        case proto::power::Control::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(Q_OS_WINDOWS)
            if (!SafeModeUtil::setSafeModeService(Service::kName, true))
            {
                CLOG(ERROR) << "Failed to add service to start in safe mode";
                return;
            }

            CLOG(INFO) << "Service added successfully to start in safe mode";

            HostStorage storage;
            storage.setBootToSafeMode(true);

            if (!SafeModeUtil::setSafeMode(true))
            {
                CLOG(ERROR) << "Failed to enable boot in Safe Mode";
                return;
            }

            CLOG(INFO) << "Safe Mode boot enabled successfully";
            if (!PowerController::reboot())
                CLOG(ERROR) << "Unable to reboot";
#endif // defined(Q_OS_WINDOWS)
        }
        break;

        case proto::power::Control::ACTION_LOGOFF:
        case proto::power::Control::ACTION_LOCK:
        {
            proto::power::ClientToHost message;
            message.mutable_power_control()->CopyFrom(control);

            quint32 channel_id = makeUint32(
                proto::desktop::IPC_CHANNEL_ID_SESSION, proto::desktop::CHANNEL_ID_POWER);

            if (ipc_channel_)
                ipc_channel_->send(channel_id, serialize(message));
        }
        break;

        default:
            CLOG(ERROR) << "Unhandled power control action:" << control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::readTaskManager(const proto::task_manager::ClientToHost& message)
{
    if (!task_manager_)
    {
        task_manager_ = new TaskManager(this);
        connect(task_manager_, &TaskManager::sig_taskManagerMessage,
                this, &DesktopClient::onTaskManagerMessage);
    }
    task_manager_->readMessage(message);
}
