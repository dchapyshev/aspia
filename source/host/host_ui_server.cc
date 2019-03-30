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

namespace {

class IsProcessInSession
{
public:
    IsProcessInSession(base::win::SessionId session_id)
        : session_id_(session_id)
    {
        // Nothing
    }

    bool operator()(const std::unique_ptr<UiProcess>& process)
    {
        return process->sessionId() == session_id_;
    }

private:
    base::win::SessionId session_id_;
};

} // namespace

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

    std::unique_ptr<ipc::Server> server(new ipc::Server());
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
    server_ = std::move(server);
    return true;
}

void UiServer::stop()
{
    if (state_ == State::STOPPED || state_ == State::STOPPING)
        return;

    state_ = State::STOPPING;

    server_.reset();
    processes_.clear();

    state_ = State::STOPPED;
    emit finished();
}

bool UiServer::hasUiForSession(base::win::SessionId session_id) const
{
    return std::any_of(processes_.cbegin(), processes_.cend(), IsProcessInSession(session_id));
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

    auto result = std::find_if(processes_.cbegin(), processes_.cend(), IsProcessInSession(session_id));
    if (result != processes_.cend())
        (*result)->setConnectEvent(event);
}

void UiServer::setDisconnectEvent(base::win::SessionId session_id, const std::string& uuid)
{
    LOG(LS_INFO) << "Disconnect event (SID: " << session_id << ", UUID: " << uuid << ")";

    auto result = std::find_if(processes_.cbegin(), processes_.cend(), IsProcessInSession(session_id));
    if (result != processes_.cend())
        (*result)->setDisconnectEvent(uuid);
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

    UiProcess* process = new UiProcess(channel);

    connect(process, &UiProcess::userChanged, this, &UiServer::onUserChanged);
    connect(process, &UiProcess::killSession, this, &UiServer::killSession);
    connect(process, &UiProcess::finished, this, &UiServer::onProcessFinished);

    processes_.emplace_front(process);
    process->start();

    emit processEvent(EventType::CONNECTED, session_id);
}

void UiServer::onProcessFinished()
{
    if (state_ == State::STOPPING)
        return;

    for (auto it = processes_.begin(); it != processes_.end();)
    {
        UiProcess* process = it->get();

        if (process->state() == UiProcess::State::STOPPED)
        {
            base::win::SessionId session_id = process->sessionId();

            auto result = users_.find(session_id);
            if (result != users_.end())
                users_.erase(result);

            emit processEvent(EventType::DISCONNECTED, session_id);

            it = processes_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    emit userListChanged();
}

void UiServer::onUserChanged(base::win::SessionId session_id, const std::string& password)
{
    net::SrpUser user = net::SrpUser::create("#" + std::to_string(session_id), password);

    user.sessions = proto::SESSION_TYPE_ALL;
    user.flags = net::SrpUser::ENABLED;

    auto result = users_.find(session_id);
    if (result != users_.end())
    {
        result->second = std::move(user);
    }
    else
    {
        users_.emplace(session_id, std::move(user));
    }

    emit userListChanged();
}

} // namespace host
