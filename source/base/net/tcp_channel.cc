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

#include "base/net/tcp_channel.h"

namespace base {

namespace {

auto g_errorCodeType = qRegisterMetaType<base::TcpChannel::ErrorCode>();

//--------------------------------------------------------------------------------------------------
int calculateSpeed(int last_speed, const TcpChannel::Milliseconds& duration, qint64 bytes)
{
    static const double kAlpha = 0.1;
    return int((kAlpha * ((1000.0 / double(duration.count())) * double(bytes))) +
        ((1.0 - kAlpha) * double(last_speed)));
}

//--------------------------------------------------------------------------------------------------
quint32 makeInstanceId()
{
    static thread_local quint32 instance_id = 0;
    ++instance_id;
    return instance_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
const quint32 TcpChannel::kMaxMessageSize = 7 * 1024 * 1024; // 7 MB

//--------------------------------------------------------------------------------------------------
TcpChannel::TcpChannel(Type type, QObject* parent)
    : QObject(parent),
      instance_id_(makeInstanceId()),
      type_(type)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
QString TcpChannel::errorToString(ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case ErrorCode::INVALID_PROTOCOL:
            message = QT_TR_NOOP("Violation of the communication protocol.");
            break;

        case ErrorCode::ACCESS_DENIED:
            message = QT_TR_NOOP("Wrong user name or password.");
            break;

        case ErrorCode::CRYPTO_ERROR:
            message = QT_TR_NOOP("Cryptography error (message encryption or decryption failed).");
            break;

        case ErrorCode::SESSION_DENIED:
            message = QT_TR_NOOP("Specified session type is not allowed for the user.");
            break;

        case ErrorCode::VERSION_ERROR:
            message = QT_TR_NOOP("Version of the application you are connecting to is less than "
                                 "the minimum supported version.");
            break;

        case ErrorCode::NETWORK_ERROR:
            message = QT_TR_NOOP("An error occurred with the network (e.g., the network cable was accidentally plugged out).");
            break;

        case ErrorCode::CONNECTION_REFUSED:
            message = QT_TR_NOOP("Connection was refused by the peer (or timed out).");
            break;

        case ErrorCode::REMOTE_HOST_CLOSED:
            message = QT_TR_NOOP("Remote host closed the connection.");
            break;

        case ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            message = QT_TR_NOOP("Host address was not found.");
            break;

        case ErrorCode::SOCKET_TIMEOUT:
            message = QT_TR_NOOP("Socket operation timed out.");
            break;

        case ErrorCode::ADDRESS_IN_USE:
            message = QT_TR_NOOP("Address specified is already in use and was set to be exclusive.");
            break;

        case ErrorCode::ADDRESS_NOT_AVAILABLE:
            message = QT_TR_NOOP("Address specified does not belong to the host.");
            break;

        default:
        {
            if (error_code != ErrorCode::UNKNOWN)
            {
                LOG(ERROR) << "Unknown error code:" << static_cast<int>(error_code);
            }

            message = QT_TR_NOOP("An unknown error occurred.");
        }
        break;
    }

    return tr(message);
}

//--------------------------------------------------------------------------------------------------
int TcpChannel::speedRx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_rx_);

    speed_rx_ = calculateSpeed(speed_rx_, duration, bytes_rx_);
    begin_time_rx_ = current_time;
    bytes_rx_ = 0;

    return speed_rx_;
}

//--------------------------------------------------------------------------------------------------
int TcpChannel::speedTx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_tx_);

    speed_tx_ = calculateSpeed(speed_tx_, duration, bytes_tx_);
    begin_time_tx_ = current_time;
    bytes_tx_ = 0;

    return speed_tx_;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::addTxBytes(size_t bytes_count)
{
    bytes_tx_ += bytes_count;
    total_tx_ += bytes_count;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::addRxBytes(size_t bytes_count)
{
    bytes_rx_ += bytes_count;
    total_rx_ += bytes_count;
}

} // namespace base
