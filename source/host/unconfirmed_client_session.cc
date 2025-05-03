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

#include "host/unconfirmed_client_session.h"

#include "base/logging.h"
#include "host/client_session.h"

namespace host {

namespace {

const std::chrono::seconds kRejectInterval { 60 };

} // namespace

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::UnconfirmedClientSession(std::unique_ptr<ClientSession> client_session,
                                                   Delegate* delegate,
                                                   QObject* parent)
    : QObject(parent),
      delegate_(delegate),
      client_session_(std::move(client_session)),
      timer_(std::make_unique<QTimer>())
{
    LOG(LS_INFO) << "Ctor";

    DCHECK(delegate_);
    DCHECK(client_session_);

    id_ = client_session_->id();
    timer_->setSingleShot(true);
}

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::~UnconfirmedClientSession()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UnconfirmedClientSession::setTimeout(const std::chrono::milliseconds& timeout)
{
    LOG(LS_INFO) << "Timeout changed: " << timeout.count();

    if (timeout <= std::chrono::milliseconds(0))
    {
        // An interval set to 0 means that automatic confirmation is disabled. We start a timer that
        // counts down until the connection is automatically rejected. If the user does not approve
        // the connection within this time, then it will be automatically rejected.
        connect(timer_.get(), &QTimer::timeout, this, [this]()
        {
            delegate_->onUnconfirmedSessionReject(id());
        });

        timer_->start(kRejectInterval);
    }
    else
    {
        // If the user does not approve the connection within the specified time, then it will be
        // automatically accepted.
        connect(timer_.get(), &QTimer::timeout, this, [this]()
        {
            delegate_->onUnconfirmedSessionAccept(id());
        });

        timer_->start(timeout);
    }
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<ClientSession> UnconfirmedClientSession::takeClientSession()
{
    return std::move(client_session_);
}

//--------------------------------------------------------------------------------------------------
quint32 UnconfirmedClientSession::id() const
{
    return id_;
}

} // namespace host
