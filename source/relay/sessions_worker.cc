//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/strings/unicode.h"

namespace relay {

SessionsWorker::SessionsWorker(std::u16string_view listen_interface,
                               uint16_t peer_port,
                               const std::chrono::minutes& peer_idle_timeout,
                               bool statistics_enabled,
                               const std::chrono::seconds& statistics_interval,
                               std::unique_ptr<SharedPool> shared_pool)
    : listen_interface_(listen_interface),
      peer_port_(peer_port),
      peer_idle_timeout_(peer_idle_timeout),
      statistics_enabled_(statistics_enabled),
      statistics_interval_(statistics_interval),
      shared_pool_(std::move(shared_pool)),
      thread_(std::make_unique<base::Thread>())
{
    DCHECK(peer_port_ && shared_pool_);
}

SessionsWorker::~SessionsWorker()
{
    thread_->stop();
}

void SessionsWorker::start(std::shared_ptr<base::TaskRunner> caller_task_runner,
                           SessionManager::Delegate* delegate)
{
    caller_task_runner_ = std::move(caller_task_runner);
    delegate_ = delegate;

    DCHECK(caller_task_runner_);
    DCHECK(delegate_);

    thread_->start(base::MessageLoop::Type::ASIO, this);
}

void SessionsWorker::onBeforeThreadRunning()
{
    self_task_runner_ = thread_->taskRunner();
    DCHECK(self_task_runner_);

    asio::ip::address listen_address =
        asio::ip::make_address_v4(base::local8BitFromUtf16(listen_interface_));

    session_manager_ = std::make_unique<SessionManager>(
        self_task_runner_, listen_address, peer_port_, peer_idle_timeout_, statistics_enabled_,
        statistics_interval_);
    session_manager_->start(std::move(shared_pool_), this);
}

void SessionsWorker::onAfterThreadRunning()
{
    session_manager_.reset();
}

void SessionsWorker::onSessionStatistics(const proto::RelayStat& relay_stat)
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(
            std::bind(&SessionsWorker::onSessionStatistics, this, relay_stat));
        return;
    }

    if (delegate_)
        delegate_->onSessionStatistics(relay_stat);
}

void SessionsWorker::onSessionFinished()
{
    if (!caller_task_runner_->belongsToCurrentThread())
    {
        caller_task_runner_->postTask(std::bind(&SessionsWorker::onSessionFinished, this));
        return;
    }

    if (delegate_)
        delegate_->onSessionFinished();
}

} // namespace relay
