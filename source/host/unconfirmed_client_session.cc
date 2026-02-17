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

#include "host/unconfirmed_client_session.h"

#include "base/logging.h"
#include "host/client_session.h"

namespace host {

namespace {

const std::chrono::seconds kRejectInterval { 60 };

} // namespace

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::UnconfirmedClientSession(ClientSession* client_session, QObject* parent)
    : QObject(parent),
      client_session_(client_session),
      timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    DCHECK(client_session_);

    client_session_->setParent(this);
    id_ = client_session_->id();
    timer_->setSingleShot(true);
}

//--------------------------------------------------------------------------------------------------
UnconfirmedClientSession::~UnconfirmedClientSession()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UnconfirmedClientSession::setTimeout(const std::chrono::milliseconds& timeout)
{
    LOG(INFO) << "Timeout changed:" << timeout.count();

    if (timeout <= std::chrono::milliseconds(0))
    {
        // An interval set to 0 means that automatic confirmation is disabled. We start a timer that
        // counts down until the connection is automatically rejected. If the user does not approve
        // the connection within this time, then it will be automatically rejected.
        connect(timer_, &QTimer::timeout, this, [this]()
        {
            emit sig_finished(id(), true); // Rejected.
        });

        timer_->start(kRejectInterval);
    }
    else
    {
        // If the user does not approve the connection within the specified time, then it will be
        // automatically accepted.
        connect(timer_, &QTimer::timeout, this, [this]()
        {
            emit sig_finished(id(), false); // Accepted.
        });

        timer_->start(timeout);
    }
}

//--------------------------------------------------------------------------------------------------
ClientSession* UnconfirmedClientSession::takeClientSession()
{
    if (!client_session_)
        return nullptr;

    ClientSession* client_session = client_session_;
    client_session_ = nullptr;

    client_session->setParent(nullptr);
    return client_session;
}

//--------------------------------------------------------------------------------------------------
quint32 UnconfirmedClientSession::id() const
{
    return id_;
}

} // namespace host
