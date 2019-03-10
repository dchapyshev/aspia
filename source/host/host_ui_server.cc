//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_ui_server.h"
#include "base/logging.h"
#include "base/win/session_enumerator.h"
#include "host/host_ui_constants.h"
#include "host/host_ui_process.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_server.h"
#include "net/srp_host_context.h"

namespace host {

UiServer::UiServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

UiServer::~UiServer()
{
    stop();
}

bool UiServer::start()
{
    if (state_ != State::STOPPED)
    {
        DLOG(LS_WARNING) << "Attempting to start an already running server";
        return false;
    }

    std::unique_ptr<ipc::Server> server(new ipc::Server(this));
    server->setChannelId(kIpcChannelIdForUI);

    connect(server.get(), &ipc::Server::newConnection, this, &UiServer::onChannelConnected);

    if (!server->start())
        return false;

    for (base::win::SessionEnumerator session; !session.isAtEnd(); session.advance())
    {
        base::win::SessionId session_id = session.sessionId();

        if (session_id != base::win::kServiceSessionId)
            UiProcess::create(session_id);
    }

    state_ = State::STARTED;
    server_ = server.release();
    return true;
}

void UiServer::stop()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    state_ = State::STOPPING;

    if (server_)
    {
        server_->stop();
        delete server_;
    }

    for (auto process : process_list_)
    {
        process->stop();
        delete process;
    }

    process_list_.clear();

    state_ = State::STOPPED;
    emit finished();
}

bool UiServer::hasUiForSession(base::win::SessionId session_id) const
{
    for (const auto process : process_list_)
    {
        if (process->sessionId() == session_id)
            return true;
    }

    return false;
}

void UiServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    switch (status)
    {
        case base::win::SessionStatus::SESSION_LOGON:
        {
            if (session_id != base::win::kServiceSessionId)
                UiProcess::create(session_id);
        }
        break;

        default:
            break;
    }
}

void UiServer::setConnectEvent(
    base::win::SessionId session_id, const proto::host::ConnectEvent& event)
{
    LOG(LS_INFO) << "Connect event (SID: " << session_id << ", UUID: " << event.uuid() << ")";

    for (auto process : process_list_)
    {
        if (process->sessionId() == session_id)
        {
            process->setConnectEvent(event);
            break;
        }
    }
}

void UiServer::setDisconnectEvent(base::win::SessionId session_id, const std::string& uuid)
{
    LOG(LS_INFO) << "Disconnect event (SID: " << session_id << ", UUID: " << uuid << ")";

    for (auto process : process_list_)
    {
        if (process->sessionId() == session_id)
        {
            process->setDisconnectEvent(uuid);
            break;
        }
    }
}

void UiServer::onChannelConnected(ipc::Channel* channel)
{
    base::win::SessionId session_id = channel->clientSessionId();

    if (hasUiForSession(session_id))
    {
        LOG(LS_ERROR) << "There is already a running UI process for the session " << session_id;
        channel->stop();
        channel->deleteLater();
        return;
    }

    LOG(LS_INFO) << "Process has been successfully connected ("
                 << "PID: " << channel->clientProcessId() << ", "
                 << "SID: " << session_id << ")";

    UiProcess* process = new UiProcess(channel, this);

    connect(process, &UiProcess::userChanged, this, &UiServer::onUserChanged);
    connect(process, &UiProcess::killSession, this, &UiServer::killSession);
    connect(process, &UiProcess::finished, this, &UiServer::onProcessFinished);

    process_list_.emplace_back(process);
    process->start();

    emit processEvent(ProcessEvent::CONNECTED, session_id);
}

void UiServer::onProcessFinished()
{
    if (state_ == State::STOPPING)
        return;

    for (auto it = process_list_.begin(); it != process_list_.end();)
    {
        UiProcess* process = *it;

        if (process->state() == UiProcess::State::STOPPED)
        {
            base::win::SessionId session_id = process->sessionId();

            auto result = user_list_.find(session_id);
            if (result != user_list_.end())
                user_list_.erase(result);

            emit processEvent(ProcessEvent::DISCONNECTED, session_id);

            it = process_list_.erase(it);
            delete process;
        }
        else
        {
            ++it;
        }
    }

    emit userListChanged();
}

void UiServer::onUserChanged(base::win::SessionId session_id,
                             const std::string& username,
                             const std::string& password)
{
    std::unique_ptr<net::SrpUser> user(
        net::SrpHostContext::createUser(QString::fromStdString(username),
                                        QString::fromStdString(password)));

    auto result = user_list_.find(session_id);
    if (result != user_list_.end())
    {
        result->second = std::move(*user);
    }
    else
    {
        user_list_.emplace(session_id, std::move(*user));
    }

    emit userListChanged();
}

} // namespace host
