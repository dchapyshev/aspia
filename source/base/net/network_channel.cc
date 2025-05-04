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

#include "base/net/network_channel.h"

namespace base {

namespace {

auto g_errorCodeType = qRegisterMetaType<base::NetworkChannel::ErrorCode>();

//--------------------------------------------------------------------------------------------------
int calculateSpeed(int last_speed, const NetworkChannel::Milliseconds& duration, qint64 bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(bytes))) +
        ((1.0 - kAlpha) * static_cast<double>(last_speed)));
}

} // namespace

const quint32 NetworkChannel::kMaxMessageSize = 7 * 1024 * 1024; // 7 MB

//--------------------------------------------------------------------------------------------------
int NetworkChannel::speedRx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_rx_);

    speed_rx_ = calculateSpeed(speed_rx_, duration, bytes_rx_);

    begin_time_rx_ = current_time;
    bytes_rx_ = 0;

    return speed_rx_;
}

//--------------------------------------------------------------------------------------------------
int NetworkChannel::speedTx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_tx_);

    speed_tx_ = calculateSpeed(speed_tx_, duration, bytes_tx_);

    begin_time_tx_ = current_time;
    bytes_tx_ = 0;

    return speed_tx_;
}

//--------------------------------------------------------------------------------------------------
// static
QString NetworkChannel::errorToString(ErrorCode error_code)
{
    const char* str;

    switch (error_code)
    {
        case ErrorCode::SUCCESS:
            str = "SUCCESS";
            break;

        case ErrorCode::INVALID_PROTOCOL:
            str = "INVALID_PROTOCOL";
            break;

        case ErrorCode::ACCESS_DENIED:
            str = "ACCESS_DENIED";
            break;

        case ErrorCode::NETWORK_ERROR:
            str = "NETWORK_ERROR";
            break;

        case ErrorCode::CONNECTION_REFUSED:
            str = "CONNECTION_REFUSED";
            break;

        case ErrorCode::REMOTE_HOST_CLOSED:
            str = "REMOTE_HOST_CLOSED";
            break;

        case ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            str = "SPECIFIED_HOST_NOT_FOUND";
            break;

        case ErrorCode::SOCKET_TIMEOUT:
            str = "SOCKET_TIMEOUT";
            break;

        case ErrorCode::ADDRESS_IN_USE:
            str = "ADDRESS_IN_USE";
            break;

        case ErrorCode::ADDRESS_NOT_AVAILABLE:
            str = "ADDRESS_NOT_AVAILABLE";
            break;

        default:
            str = "UNKNOWN";
            break;
    }

    return QString("%1 (%2)").arg(str).arg(static_cast<int>(error_code));
}

//--------------------------------------------------------------------------------------------------
void NetworkChannel::addTxBytes(size_t bytes_count)
{
    bytes_tx_ += bytes_count;
    total_tx_ += bytes_count;
}

//--------------------------------------------------------------------------------------------------
void NetworkChannel::addRxBytes(size_t bytes_count)
{
    bytes_rx_ += bytes_count;
    total_rx_ += bytes_count;
}

//--------------------------------------------------------------------------------------------------
// static
void NetworkChannel::resizeBuffer(QByteArray* buffer, size_t new_size)
{
    // If the reserved buffer size is less, then increase it.
    if (buffer->capacity() < static_cast<QByteArray::size_type>(new_size))
    {
        buffer->clear();
        buffer->reserve(static_cast<QByteArray::size_type>(new_size));
    }

    // Change the size of the buffer.
    buffer->resize(static_cast<QByteArray::size_type>(new_size));
}

} // namespace base
