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

#include "proxy/pending_session.h"

#include "base/endian.h"

#include <asio/read.hpp>

namespace proxy {

namespace {

constexpr std::chrono::seconds kTimeout{ 30 };

} // namespace

PendingSession::PendingSession(std::shared_ptr<base::TaskRunner> task_runner,
                               asio::ip::tcp::socket&& socket,
                               Delegate* delegate)
    : delegate_(delegate),
      timer_(std::move(task_runner)),
      socket_(std::move(socket))
{
    // Nothing
}

PendingSession::~PendingSession()
{
    stop();
}

void PendingSession::start()
{
    timer_.start(kTimeout, std::bind(&PendingSession::onErrorOccurred, this));
    PendingSession::doReadMessage(this);
}

void PendingSession::stop()
{
    if (!delegate_)
        return;

    delegate_ = nullptr;
    timer_.stop();

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

void PendingSession::setIdentify(const PeerIdPair& id_pair, uint32_t key_id)
{
    id_pair_ = id_pair;
    key_id_ = key_id;
}

bool PendingSession::isPeerFor(const PendingSession& other) const
{
    if (!id_pair_.has_value() || !other.id_pair_.has_value())
        return false;

    return other.id_pair_->first == id_pair_->second &&
           other.id_pair_->second == id_pair_->first &&
           key_id_ == other.key_id_;
}

asio::ip::tcp::socket PendingSession::takeSocket()
{
    return std::move(socket_);
}

// static
void PendingSession::doReadMessage(PendingSession* session)
{
    asio::async_read(session->socket_,
                     asio::buffer(&session->buffer_size_, sizeof(uint32_t)),
                     [session](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                session->onErrorOccurred();
            return;
        }

        session->buffer_size_ = base::Endian::fromBig(session->buffer_size_);
        if (!session->buffer_size_ || session->buffer_size_ > session->buffer_.size())
        {
            session->onErrorOccurred();
            return;
        }

        asio::async_read(session->socket_,
                         asio::buffer(session->buffer_.data(), session->buffer_size_),
                         [session](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    session->onErrorOccurred();
                return;
            }

            session->onMessage();
        });
    });
}

void PendingSession::onErrorOccurred()
{
    if (delegate_)
        delegate_->onPendingSessionFailed(this);

    terminate();
}

void PendingSession::onMessage()
{
    proto::PeerToProxy message;
    if (!message.ParseFromArray(buffer_.data(), buffer_.size()))
    {
        onErrorOccurred();
        return;
    }

    if (delegate_)
        delegate_->onPendingSessionReady(this, message);
}

} // namespace proxy
