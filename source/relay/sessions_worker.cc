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

#include "relay/sessions_worker.h"

#include "base/logging.h"

namespace relay {

//--------------------------------------------------------------------------------------------------
SessionsWorker::SessionsWorker(const QString& listen_interface,
                               quint16 peer_port,
                               const std::chrono::minutes& peer_idle_timeout,
                               bool statistics_enabled,
                               const std::chrono::seconds& statistics_interval,
                               std::unique_ptr<SharedPool> shared_pool,
                               QObject* parent)
    : QObject(parent),
      listen_interface_(listen_interface),
      peer_port_(peer_port),
      peer_idle_timeout_(peer_idle_timeout),
      statistics_enabled_(statistics_enabled),
      statistics_interval_(statistics_interval),
      shared_pool_(std::move(shared_pool)),
      thread_(std::make_unique<base::Thread>(base::Thread::AsioDispatcher, this))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(peer_port_ && shared_pool_);
}

//--------------------------------------------------------------------------------------------------
SessionsWorker::~SessionsWorker()
{
    LOG(LS_INFO) << "Dtor";
    thread_->stop();
}

//--------------------------------------------------------------------------------------------------
void SessionsWorker::start()
{
    LOG(LS_INFO) << "Starting session worker";
    thread_->start();
}

//--------------------------------------------------------------------------------------------------
void SessionsWorker::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "Before thread running";

    asio::ip::address listen_address;

    if (!listen_interface_.isEmpty())
    {
        std::error_code error_code;
        listen_address = asio::ip::make_address(
            listen_interface_.toLocal8Bit().toStdString(), error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Unable to get listen address: " << error_code;
            return;
        }
    }
    else
    {
        listen_address = asio::ip::address_v6::any();
    }

    LOG(LS_INFO) << "Listen interface: "
                 << (listen_interface_.isEmpty() ? "ANY" : listen_interface_) << ":" << peer_port_;

    session_manager_ = std::make_unique<SessionManager>(
        listen_address, peer_port_, peer_idle_timeout_, statistics_enabled_, statistics_interval_);

    connect(session_manager_.get(), &SessionManager::sig_sessionStarted,
            this, &SessionsWorker::sig_sessionStarted, Qt::QueuedConnection);
    connect(session_manager_.get(), &SessionManager::sig_sessionFinished,
            this, &SessionsWorker::sig_sessionFinished, Qt::QueuedConnection);
    connect(session_manager_.get(), &SessionManager::sig_sessionStatistics,
            this, &SessionsWorker::sig_sessionStatistics, Qt::QueuedConnection);
    connect(this, &SessionsWorker::sig_disconnectSession,
            session_manager_.get(), &SessionManager::disconnectSession, Qt::QueuedConnection);

    session_manager_->start(std::move(shared_pool_));
}

//--------------------------------------------------------------------------------------------------
void SessionsWorker::onAfterThreadRunning()
{
    LOG(LS_INFO) << "After thread running";
    session_manager_.reset();
}

} // namespace relay
