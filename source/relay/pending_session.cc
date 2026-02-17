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

#include "relay/pending_session.h"

#include <QtEndian>

#include "base/location.h"
#include "base/logging.h"

#include <asio/read.hpp>

namespace relay {

namespace {

constexpr std::chrono::seconds kTimeout{ 30 };
constexpr quint32 kMaxMessageSize = 1 * 1024 * 1024; // 1 MB

} // namespace

//--------------------------------------------------------------------------------------------------
PendingSession::PendingSession(asio::ip::tcp::socket&& socket, QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)),
      socket_(std::move(socket))
{
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]()
    {
        onErrorOccurred(FROM_HERE, std::error_code());
    });

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            LOG(ERROR) << "Unable to get endpoint for pending session:" << error_code;
        }
        else
        {
            std::string address = endpoint.address().to_string();
            address_ = QString::fromLocal8Bit(
                address.c_str(), static_cast<QString::size_type>(address.size()));
        }
    }
    catch (const std::error_code& error_code)
    {
        LOG(ERROR) << "Unable to get address for pending session:" << error_code;
    }
}

//--------------------------------------------------------------------------------------------------
PendingSession::~PendingSession()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
void PendingSession::start()
{
    LOG(INFO) << "Starting pending session";

    start_time_ = Clock::now();

    asio::ip::tcp::no_delay option(true);
    asio::error_code error_code;

    socket_.set_option(option, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Failed to disable Nagle's algorithm:" << error_code;
    }

    timer_->start(kTimeout);
    PendingSession::doReadMessage(this);
}

//--------------------------------------------------------------------------------------------------
void PendingSession::stop()
{
    timer_->stop();

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void PendingSession::setIdentify(quint32 key_id, const QByteArray& secret)
{
    secret_ = secret;
    key_id_ = key_id;
}

//--------------------------------------------------------------------------------------------------
bool PendingSession::isPeerFor(const PendingSession& other) const
{
    if (&other == this)
        return false;

    if (secret_.isEmpty() || other.secret_.isEmpty())
        return false;

    return key_id_ == other.key_id_ && secret_ == other.secret_;
}

//--------------------------------------------------------------------------------------------------
asio::ip::tcp::socket PendingSession::takeSocket()
{
    return std::move(socket_);
}

//--------------------------------------------------------------------------------------------------
const QString& PendingSession::address() const
{
    return address_;
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds PendingSession::duration(const TimePoint& now) const
{
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
}

//--------------------------------------------------------------------------------------------------
quint32 PendingSession::keyId() const
{
    return key_id_;
}

//--------------------------------------------------------------------------------------------------
// static
void PendingSession::doReadMessage(PendingSession* session)
{
    asio::async_read(session->socket_,
                     asio::buffer(&session->buffer_size_, sizeof(quint32)),
                     [session](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                session->onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        if (bytes_transferred != sizeof(quint32))
        {
            session->onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        session->buffer_size_ = qFromBigEndian(session->buffer_size_);
        if (!session->buffer_size_ || session->buffer_size_ > kMaxMessageSize)
        {
            session->onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        session->buffer_.resize(session->buffer_size_);

        asio::async_read(session->socket_,
                         asio::buffer(session->buffer_.data(), session->buffer_.size()),
                         [session](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    session->onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            if (bytes_transferred != session->buffer_.size())
            {
                session->onErrorOccurred(FROM_HERE, std::error_code());
                return;
            }

            session->onMessage();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void PendingSession::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    LOG(ERROR) << "Connection error:" << error_code << "(" << location << ")";
    emit sig_pendingSessionFailed(this);
    stop();
}

//--------------------------------------------------------------------------------------------------
void PendingSession::onMessage()
{
    proto::relay::PeerToRelay message;
    if (!message.ParseFromArray(buffer_.data(), static_cast<int>(buffer_.size())))
    {
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    emit sig_pendingSessionReady(this, message);
}

} // namespace relay
