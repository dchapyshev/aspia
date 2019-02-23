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
        DLOG(LS_WARNING) << "Attempting to start an already running server.";
        return false;
    }

    std::unique_ptr<ipc::Server> server(new ipc::Server(this));
    server->setChannelId(kIpcChannelIdForUI);

    connect(server.get(), &ipc::Server::newConnection, this, &UiServer::onChannelConnected);

    if (!server->start())
        return false;

    for (base::win::SessionEnumerator session; !session.isAtEnd(); session.advance())
        UiProcess::create(session.sessionId());

    state_ = State::STARTED;
    server_ = server.release();
    return true;
}

void UiServer::stop()
{
    if (state_ == State::STOPPED)
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

void UiServer::setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id)
{
    switch (status)
    {
        case base::win::SessionStatus::SESSION_LOGON:
            UiProcess::create(session_id);
            break;

        default:
            break;
    }
}

void UiServer::setConnectEvent(
    base::win::SessionId session_id, const proto::notifier::ConnectEvent& event)
{
    LOG(LS_INFO) << "Connect event (SID: " << session_id << ").";

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
    LOG(LS_INFO) << "Disconnect event (SID: " << session_id << ").";

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
    LOG(LS_INFO) << "Process has been successfully connected (PID: "
                 << channel->clientProcessId() << ", SID: "
                 << channel->clientSessionId() << ").";

    UiProcess* process = new UiProcess(channel, this);

    connect(process, &UiProcess::finished, [this]()
    {
        if (state_ == State::STOPPING)
            return;

        auto it = process_list_.begin();

        while (it != process_list_.end())
        {
            UiProcess* process = *it;

            if (process->state() == UiProcess::State::STOPPED)
            {
                it = process_list_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    });

    process_list_.emplace_back(process);
    process->start();
}

} // namespace host
