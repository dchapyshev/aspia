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

#include <QVariant>

#include "base/application.h"
#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "proto/desktop_internal.h"
#include "proto/desktop_service.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/session_enumerator.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopClient::DesktopClient(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    setProperty("version", tcp_channel_->peerVersion().toString());
    setProperty("os_name", tcp_channel_->peerOsName());
    setProperty("session_type", tcp_channel_->peerSessionType());
    setProperty("user_name", tcp_channel_->peerUserName());
    setProperty("display_name", tcp_channel_->peerDisplayName());
    setProperty("computer_name", tcp_channel_->peerComputerName());
    setProperty("arch", tcp_channel_->peerArchitecture());

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &DesktopClient::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &DesktopClient::onTcpMessageReceived);

#if defined(Q_OS_WINDOWS)
    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &DesktopClient::sendSessionList);
#endif // defined(Q_OS_WINDOWS)

    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(WARNING) << "Timeout when desktop client starting";
        emit sig_finished(clientId());
    });

    attach_timer_->setInterval(std::chrono::seconds(15));
    attach_timer_->start();
}

//--------------------------------------------------------------------------------------------------
DesktopClient::~DesktopClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
quint32 DesktopClient::clientId() const
{
    return tcp_channel_->instanceId();
}

//--------------------------------------------------------------------------------------------------
bool DesktopClient::isAttached() const
{
    return ipc_channel_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
QString DesktopClient::attach()
{
    CHECK(!ipc_channel_);
    CHECK(!ipc_server_);

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &DesktopClient::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred, this, &DesktopClient::onIpcErrorOccurred);

    QString channel_name = base::IpcServer::createUniqueId();

    if (!ipc_server_->start(channel_name))
    {
        LOG(ERROR) << "Unable to start IPC server";
        emit sig_finished(clientId());
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

    attach_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::start()
{
    emit sig_started(clientId());
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcNewConnection()
{
    CHECK(ipc_server_);
    CHECK(!ipc_channel_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending IPC connections";
        emit sig_finished(clientId());
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    ipc_server_->disconnect();
    ipc_server_->deleteLater();
    ipc_server_ = nullptr;

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopClient::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopClient::onIpcMessageReceived);

    proto::peer::SessionType session_type =
        static_cast<proto::peer::SessionType>(tcp_channel_->peerSessionType());
    CHECK(session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE ||
          session_type == proto::peer::SESSION_TYPE_DESKTOP_VIEW);

    proto::desktop::ServiceToAgentClient message;
    proto::desktop::Description* description = message.mutable_description();

    description->set_session_type(session_type);
    *description->mutable_version() = base::serialize(tcp_channel_->peerVersion());
    description->set_os_name(tcp_channel_->peerOsName().toStdString());
    description->set_computer_name(tcp_channel_->peerComputerName().toStdString());
    description->set_display_name(tcp_channel_->peerDisplayName().toStdString());
    description->set_architecture(tcp_channel_->peerArchitecture().toStdString());
    description->set_user_name(tcp_channel_->peerUserName().toStdString());

    sendIpcServiceMessage(base::serialize(message));

    tcp_channel_->resume();
    ipc_channel_->resume();
    attach_timer_->stop();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcErrorOccurred()
{
    LOG(ERROR) << "Error in IPC server";
    emit sig_finished(clientId());
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer)
{
    quint16 tcp_channel_id = base::lowWord(channel_id);
    quint16 ipc_channel_id = base::highWord(channel_id);

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
    {
        tcp_channel_->send(tcp_channel_id, buffer);
    }
    else
    {
        // TODO: Handle service message.
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcDisconnected()
{
    if (!ipc_channel_)
        return;

    LOG(INFO) << "IPC channel disconnected";
    dettach();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(ERROR) << "TCP error:" << error_code;
    CHECK(tcp_channel_);

    tcp_channel_->disconnect();

    if (ipc_channel_)
    {
        ipc_channel_->disconnect();
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    emit sig_finished(clientId());
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    if (tcp_channel_id == proto::peer::CHANNEL_ID_SESSION)
    {
        quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, tcp_channel_id);

        if (ipc_channel_)
            ipc_channel_->send(channel_id, buffer);
    }
    else if (tcp_channel_id == proto::peer::CHANNEL_ID_SERVICE)
    {
        proto::desktop::ClientToService message;

        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse service message";
            return;
        }

        if (message.has_session_list_request())
        {
            sendSessionList();
        }
        else if (message.has_switch_session())
        {
            if (tcp_channel_->peerSessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
            {
                LOG(ERROR) << "Switch session available only for desktop manage session type";
                return;
            }

            base::SessionId session_id = message.switch_session().session_id();
            if (session_id == base::kInvalidSessionId || session_id == base::kServiceSessionId)
            {
                LOG(ERROR) << "Invalid session id:" << session_id;
                return;
            }

            LOG(INFO) << "Switch session received:" << session_id;
            emit sig_switchSession(session_id);
        }
        else if (message.has_video_recording())
        {
            bool is_started =
                message.video_recording().action() == proto::desktop::VideoRecording::ACTION_STARTED;
            emit sig_recordingChanged(tcp_channel_->peerComputerName(), tcp_channel_->peerUserName(), is_started);
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from channel" << tcp_channel_id;
    }
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
    proto::desktop::ServiceToClient message;
    proto::desktop::SessionList* session_list = message.mutable_session_list();

    session_list->set_current_session_id(base::currentProcessSessionId());

    for (base::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (it.sessionId() == 0) // Don't add system session.
            continue;

        proto::desktop::Session* session = session_list->add_session();

        session->set_session_id(it.sessionId());
        session->set_user_name(it.userName().toStdString());
        session->set_session_name(it.sessionName().toStdString());
        session->set_domain_name(it.domainName().toStdString());
        session->set_is_console(it.isConsole());
        session->set_is_locked(it.isUserLocked());
        session->set_is_active(it.isActive());
    }

    tcp_channel_->send(proto::peer::CHANNEL_ID_SERVICE, base::serialize(message));
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
