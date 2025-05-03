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

#include "relay/session.h"

#include "base/location.h"
#include "base/logging.h"
#include "proto/relay_peer.pb.h"

#include <asio/write.hpp>

namespace relay {

//--------------------------------------------------------------------------------------------------
Session::Session(std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>&& sockets,
                 const QByteArray& secret)
    : socket_{ std::move(sockets.first), std::move(sockets.second) }
{
    static quint64 session_id = 0;
    ++session_id;
    session_id_ = session_id;

    proto::PeerToRelay::Secret secret_message;
    if (secret_message.ParseFromArray(secret.data(), static_cast<int>(secret.size())))
    {
        client_address_ = secret_message.client_address();
        client_user_name_ = secret_message.client_user_name();
        host_address_ = secret_message.host_address();
        host_id_ = secret_message.host_id();
    }

    for (size_t i = 0; i < kNumberOfSides; ++i)
        std::fill(buffer_[i].begin(), buffer_[i].end(), 0);
}

//--------------------------------------------------------------------------------------------------
Session::~Session()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
void Session::start(Delegate* delegate)
{
    LOG(LS_INFO) << "Starting peers session";

    start_time_ = Clock::now();
    delegate_ = delegate;

    for (int i = 0; i < kNumberOfSides; ++i)
        Session::doReadSome(this, i);
}

//--------------------------------------------------------------------------------------------------
void Session::stop()
{
    if (!delegate_)
        return;

    delegate_ = nullptr;
    disconnect();

    LOG(LS_INFO) << "Session stopped (duration: " << duration(Clock::now()).count()
                 << " seconds, bytes transferred: " << bytesTransferred() << ")";
}

//--------------------------------------------------------------------------------------------------
void Session::disconnect()
{
    std::error_code ignored_code;
    for (int i = 0; i < kNumberOfSides; ++i)
    {
        socket_[i].cancel(ignored_code);
        socket_[i].close(ignored_code);
    }

    if (delegate_)
        delegate_->onSessionFinished(this);
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Session::idleTime(const TimePoint& current_time) const
{
    if (start_idle_time_ == TimePoint())
    {
        start_idle_time_ = current_time;
        return std::chrono::seconds(0);
    }

    return std::chrono::duration_cast<std::chrono::seconds>(current_time - start_idle_time_);
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Session::duration(const TimePoint& current_time) const
{
    return std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time_);
}

//--------------------------------------------------------------------------------------------------
// static
void Session::doReadSome(Session* session, int source)
{
    session->socket_[source].async_read_some(
        asio::buffer(session->buffer_[source].data(), session->buffer_[source].size()),
        [session, source](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                session->onErrorOccurred(FROM_HERE, error_code);
        }
        else
        {
            session->bytes_transferred_ += bytes_transferred;
            session->start_idle_time_ = TimePoint();

            asio::async_write(
                session->socket_[(source + kNumberOfSides - 1) % kNumberOfSides],
                asio::const_buffer(session->buffer_[source].data(), bytes_transferred),
                [session, source](const std::error_code& error_code, size_t /* bytes_transferred */)
            {
                if (error_code)
                {
                    if (error_code != asio::error::operation_aborted)
                        session->onErrorOccurred(FROM_HERE, error_code);
                }
                else
                {
                    doReadSome(session, source);
                }
            });
        }
    });
}

//--------------------------------------------------------------------------------------------------
void Session::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    LOG(LS_ERROR) << "Connection finished: " << error_code << " (" << location.toString() << ")";
    if (delegate_)
        delegate_->onSessionFinished(this);

    stop();
}

} // namespace relay
