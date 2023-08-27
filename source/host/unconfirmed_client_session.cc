//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/unconfirmed_client_session.h"

#include "base/logging.h"
#include "base/waitable_timer.h"
#include "host/client_session.h"

namespace host {

namespace {

const std::chrono::seconds kRejectInterval { 60 };

} // namespace

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::UnconfirmedClientSession(std::unique_ptr<ClientSession> client_session,
                                                   std::shared_ptr<base::TaskRunner> task_runner,
                                                   Delegate* delegate)
    : delegate_(delegate),
      client_session_(std::move(client_session)),
      timer_(std::make_unique<base::WaitableTimer>(
          base::WaitableTimer::Type::SINGLE_SHOT, std::move(task_runner)))
{
    LOG(LS_INFO) << "Ctor";

    DCHECK(delegate_);
    DCHECK(client_session_);

    id_ = client_session_->id();
}

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::~UnconfirmedClientSession()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UnconfirmedClientSession::setTimeout(const std::chrono::milliseconds& timeout)
{
    if (timeout <= std::chrono::milliseconds(0))
    {
        // An interval set to 0 means that automatic confirmation is disabled. We start a timer that
        // counts down until the connection is automatically rejected. If the user does not approve
        // the connection within this time, then it will be automatically rejected.
        timer_->start(kRejectInterval, [this]()
        {
            delegate_->onUnconfirmedSessionReject(id());
        });
    }
    else
    {
        // If the user does not approve the connection within the specified time, then it will be
        // automatically accepted.
        timer_->start(timeout, [this]()
        {
            delegate_->onUnconfirmedSessionAccept(id());
        });
    }
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<ClientSession> UnconfirmedClientSession::takeClientSession()
{
    return std::move(client_session_);
}

//--------------------------------------------------------------------------------------------------
uint32_t UnconfirmedClientSession::id() const
{
    return id_;
}

} // namespace host
