//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/user_session.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/unicode.h"
#include "common/message_serialization.h"
#include "crypto/password_generator.h"
#include "desktop/desktop_frame.h"
#include "host/client_session_desktop.h"
#include "host/desktop_session_proxy.h"
#include "ipc/ipc_channel_proxy.h"
#include "net/adapter_enumerator.h"
#include "proto/host.pb.h"

namespace host {

UserSession::UserSession(
    std::shared_ptr<base::TaskRunner> task_runner, std::unique_ptr<ipc::Channel> ipc_channel)
    : task_runner_(std::move(task_runner)),
      ipc_channel_(std::move(ipc_channel))
{
    DCHECK(task_runner_);
    DCHECK(ipc_channel_);

    session_id_ = ipc_channel_->peerSessionId();
}

UserSession::~UserSession() = default;

void UserSession::start(Delegate* delegate)
{
    delegate_ = delegate;
    DCHECK(delegate_);

    desktop_session_ = std::make_unique<DesktopSessionManager>(task_runner_, this);
    desktop_session_proxy_ = desktop_session_->sessionProxy();
    desktop_session_->attachSession(session_id_);

    updateCredentials();

    ipc_channel_->setListener(this);
    ipc_channel_->resume();

    delegate_->onUserSessionStarted();
}

base::win::SessionId UserSession::sessionId() const
{
    return session_id_;
}

User UserSession::user() const
{
    return User::create(base::utf16FromAscii(username_), base::utf16FromAscii(password_));
}

void UserSession::addNewSession(std::unique_ptr<ClientSession> client_session)
{
    ClientSession* session = client_session.get();
    DCHECK(session);

    clients_.emplace_back(std::move(client_session));

    switch (session->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            ClientSessionDesktop* desktop_client_session =
                static_cast<ClientSessionDesktop*>(session);

            desktop_client_session->setDesktopSessionProxy(desktop_session_proxy_);
        }
        break;

        default:
            break;
    }

    session->start(this);

    // Notify the UI of a new connection.
    sendConnectEvent(*session);
}

void UserSession::onDisconnected()
{
    LOG(LS_INFO) << "IPC DISCONNECTED";
}

void UserSession::onMessageReceived(const base::ByteArray& buffer)
{
    proto::UiToService message;

    if (!common::parseMessage(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from UI";
        return;
    }

    if (message.has_credentials_request())
    {
        proto::CredentialsRequest::Type type = message.credentials_request().type();

        if (type == proto::CredentialsRequest::NEW_PASSWORD)
        {
            updateCredentials();
        }
        else
        {
            DCHECK_EQ(type, proto::CredentialsRequest::REFRESH);
        }

        sendCredentials();
    }
    else if (message.has_kill_session())
    {
        killClientSession(message.kill_session().uuid());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from UI";
    }
}

void UserSession::onDesktopSessionStarted()
{
    for (auto& client : clients_)
    {
        sendConnectEvent(*client);
    }
}

void UserSession::onDesktopSessionStopped()
{
    clients_.clear();
}

void UserSession::onScreenCaptured(const desktop::Frame& frame)
{
    for (const auto& client : clients_)
    {
        const proto::SessionType session_type = client->sessionType();

        if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE ||
            session_type == proto::SESSION_TYPE_DESKTOP_VIEW)
        {
            ClientSessionDesktop* desktop_client =
                static_cast<ClientSessionDesktop*>(client.get());

            desktop_client->encodeFrame(frame);
        }
    }

    desktop_session_proxy_->captureScreen();
}

void UserSession::onScreenListChanged(const proto::ScreenList& list)
{
    for (const auto& client : clients_)
    {
        const proto::SessionType session_type = client->sessionType();

        if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE ||
            session_type == proto::SESSION_TYPE_DESKTOP_VIEW)
        {
            ClientSessionDesktop* desktop_client =
                static_cast<ClientSessionDesktop*>(client.get());

            desktop_client->setScreenList(list);
        }
    }
}

void UserSession::onClipboardEvent(const proto::ClipboardEvent& event)
{
    for (const auto& client : clients_)
    {
        const proto::SessionType session_type = client->sessionType();

        if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            ClientSessionDesktop* desktop_client =
                static_cast<ClientSessionDesktop*>(client.get());

            desktop_client->injectClipboardEvent(event);
        }
    }
}

void UserSession::onClientSessionFinished()
{
    for (auto it = clients_.begin(); it != clients_.end();)
    {
        ClientSession* client_session = it->get();

        if (client_session->state() == ClientSession::State::FINISHED)
        {
            // Notification of the UI about disconnecting the client.
            sendDisconnectEvent(client_session->id());

            it = clients_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void UserSession::sendConnectEvent(const ClientSession& client_session)
{
    proto::ServiceToUi message;
    proto::ConnectEvent* event = message.mutable_connect_event();

    event->set_remote_address(base::utf8FromUtf16(client_session.peerAddress()));
    event->set_username(base::utf8FromUtf16(client_session.userName()));
    event->set_session_type(client_session.sessionType());
    event->set_uuid(client_session.id());

    ipc_channel_->send(common::serializeMessage(message));
}

void UserSession::sendDisconnectEvent(const std::string& session_id)
{
    proto::ServiceToUi message;
    message.mutable_disconnect_event()->set_uuid(session_id);
    ipc_channel_->send(common::serializeMessage(message));
}

void UserSession::updateCredentials()
{
    crypto::PasswordGenerator generator;

    static const uint32_t kPasswordCharacters = crypto::PasswordGenerator::UPPER_CASE |
        crypto::PasswordGenerator::LOWER_CASE | crypto::PasswordGenerator::DIGITS;
    static const int kPasswordLength = 6;

    // TODO: Get password parameters from settings.
    generator.setCharacters(kPasswordCharacters);
    generator.setLength(kPasswordLength);

    username_ = "#" + base::numberToString(sessionId());
    password_ = generator.result();
}

void UserSession::sendCredentials()
{
    proto::ServiceToUi message;

    proto::Credentials* credentials = message.mutable_credentials();
    credentials->set_username(username_);
    credentials->set_password(password_);

    for (net::AdapterEnumerator adapters; !adapters.isAtEnd(); adapters.advance())
    {
        for (net::AdapterEnumerator::IpAddressEnumerator addresses(adapters);
             !addresses.isAtEnd(); addresses.advance())
        {
            std::string ip = addresses.address();

            if (ip != "127.0.0.1")
                credentials->add_ip(std::move(ip));
        }
    }

    ipc_channel_->send(common::serializeMessage(message));
}

void UserSession::killClientSession(const std::string& id)
{
    for (auto client = clients_.begin(); client != clients_.end(); ++client)
    {
        if (client->get()->id() == id)
        {
            clients_.erase(client);
            break;
        }
    }
}

} // namespace host
