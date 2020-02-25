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
#include "proto/host_internal.pb.h"

namespace host {

UserSession::UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                         std::unique_ptr<ipc::Channel> ipc_channel)
    : task_runner_(std::move(task_runner)),
      ipc_channel_(std::move(ipc_channel))
{
    DCHECK(task_runner_);
    DCHECK(ipc_channel_);

    type_ = UserSession::Type::CONSOLE;

    if (ipc_channel_->peerSessionId() != base::win::activeConsoleSessionId())
        type_ = UserSession::Type::RDP;

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

void UserSession::restart(std::unique_ptr<ipc::Channel> ipc_channel)
{
    ipc_channel_ = std::move(ipc_channel);
    DCHECK(ipc_channel_);

    ipc_channel_->setListener(this);
    ipc_channel_->resume();

    auto send_connection_list = [this](const ClientSessionList& list)
    {
        for (const auto& client : list)
            sendConnectEvent(*client);
    };

    send_connection_list(desktop_clients_);
    send_connection_list(file_transfer_clients_);
}

UserSession::Type UserSession::type() const
{
    return type_;
}

base::win::SessionId UserSession::sessionId() const
{
    return session_id_;
}

User UserSession::user() const
{
    User user = User::create(base::utf16FromAscii(username_), base::utf16FromAscii(password_));

    user.sessions = proto::SESSION_TYPE_ALL;
    user.flags = User::ENABLED;

    return user;
}

void UserSession::addNewSession(std::unique_ptr<ClientSession> client_session)
{
    DCHECK(client_session);

    ClientSession* client_session_ptr = client_session.get();

    switch (client_session_ptr->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            desktop_clients_.emplace_back(std::move(client_session));

            ClientSessionDesktop* desktop_client_session =
                static_cast<ClientSessionDesktop*>(client_session_ptr);

            desktop_client_session->setDesktopSessionProxy(desktop_session_proxy_);
            desktop_session_proxy_->enableSession(true);
        }
        break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
        {
            file_transfer_clients_.emplace_back(std::move(client_session));
        }
        break;

        default:
        {
            NOTREACHED();
            return;
        }
        break;
    }

    client_session_ptr->start(this);

    // Notify the UI of a new connection.
    sendConnectEvent(*client_session_ptr);
}

void UserSession::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    switch (status)
    {
        case base::win::SessionStatus::CONSOLE_CONNECT:
        {
            desktop_session_->attachSession(session_id);
            session_id_ = session_id;
        }
        break;

        case base::win::SessionStatus::CONSOLE_DISCONNECT:
        {
            desktop_session_->dettachSession();
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
}

void UserSession::onDisconnected()
{
    LOG(LS_INFO) << "IPC DISCONNECTED";
}

void UserSession::onMessageReceived(const base::ByteArray& buffer)
{
    proto::internal::UiToService message;

    if (!common::parseMessage(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from UI";
        return;
    }

    if (message.has_credentials_request())
    {
        proto::internal::CredentialsRequest::Type type = message.credentials_request().type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            updateCredentials();
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
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
    LOG(LS_INFO) << "The desktop session is connected";

    if (!desktop_clients_.empty())
        desktop_session_proxy_->enableSession(true);
}

void UserSession::onDesktopSessionStopped()
{
    LOG(LS_INFO) << "The desktop session is disconnected";

    if (type_ == Type::CONSOLE)
    {

    }
    else
    {
        DCHECK_EQ(type_, Type::RDP);
        desktop_clients_.clear();
    }
}

void UserSession::onScreenCaptured(const desktop::Frame& frame)
{
    for (const auto& client : desktop_clients_)
    {
        ClientSessionDesktop* desktop_client =
            static_cast<ClientSessionDesktop*>(client.get());

        desktop_client->encodeFrame(frame);
    }
}

void UserSession::onCursorCaptured(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    for (const auto& client : desktop_clients_)
    {
        if (client->sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            ClientSessionDesktop* desktop_client =
                static_cast<ClientSessionDesktop*>(client.get());

            desktop_client->encodeMouseCursor(mouse_cursor);
        }
    }
}

void UserSession::onScreenListChanged(const proto::ScreenList& list)
{
    for (const auto& client : desktop_clients_)
    {
        ClientSessionDesktop* desktop_client =
            static_cast<ClientSessionDesktop*>(client.get());

        desktop_client->setScreenList(list);
    }
}

void UserSession::onClipboardEvent(const proto::ClipboardEvent& event)
{
    for (const auto& client : desktop_clients_)
    {
        if (client->sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            ClientSessionDesktop* desktop_client =
                static_cast<ClientSessionDesktop*>(client.get());

            desktop_client->injectClipboardEvent(event);
        }
    }
}

void UserSession::onClientSessionFinished()
{
    auto delete_finished = [this](ClientSessionList* list)
    {
        for (auto it = list->begin(); it != list->end();)
        {
            ClientSession* client_session = it->get();

            if (client_session->state() == ClientSession::State::FINISHED)
            {
                // Notification of the UI about disconnecting the client.
                sendDisconnectEvent(client_session->id());

                // Session will be destroyed after completion of the current call.
                task_runner_->deleteSoon(std::move(*it));

                // Delete a session from the list.
                it = list->erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    delete_finished(&desktop_clients_);
    delete_finished(&file_transfer_clients_);

    if (desktop_clients_.empty())
        desktop_session_proxy_->enableSession(false);
}

void UserSession::sendConnectEvent(const ClientSession& client_session)
{
    proto::internal::ServiceToUi message;
    proto::internal::ConnectEvent* event = message.mutable_connect_event();

    event->set_remote_address(base::utf8FromUtf16(client_session.peerAddress()));
    event->set_username(base::utf8FromUtf16(client_session.userName()));
    event->set_session_type(client_session.sessionType());
    event->set_uuid(client_session.id());

    ipc_channel_->send(common::serializeMessage(message));
}

void UserSession::sendDisconnectEvent(const std::string& session_id)
{
    proto::internal::ServiceToUi message;
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
    proto::internal::ServiceToUi message;

    proto::internal::Credentials* credentials = message.mutable_credentials();
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

void UserSession::killClientSession(std::string_view id)
{
    auto stop_by_id = [](ClientSessionList* list, std::string_view id)
    {
        for (const auto& client_session : *list)
        {
            if (client_session->id() == id)
            {
                client_session->stop();
                break;
            }
        }
    };

    stop_by_id(&desktop_clients_, id);
    stop_by_id(&file_transfer_clients_, id);
}

} // namespace host
