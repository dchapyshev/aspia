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

#ifndef BASE_NET_NETWORK_CHANNEL_H
#define BASE_NET_NETWORK_CHANNEL_H

#include <chrono>

#include <QByteArray>
#include <QObject>

namespace base {

class NetworkChannel : public QObject
{
    Q_OBJECT

public:
    static const uint32_t kMaxMessageSize;

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    enum class ErrorCode
    {
        // Unknown error.
        UNKNOWN,

        // No error.
        SUCCESS,

        // Violation of the communication protocol.
        INVALID_PROTOCOL,

        // Cryptography error (message encryption or decryption failed).
        ACCESS_DENIED,

        // An error occurred with the network (e.g., the network cable was accidentally plugged out).
        NETWORK_ERROR,

        // The connection was refused by the peer (or timed out).
        CONNECTION_REFUSED,

        // The remote host closed the connection.
        REMOTE_HOST_CLOSED,

        // The host address was not found.
        SPECIFIED_HOST_NOT_FOUND,

        // The socket operation timed out.
        SOCKET_TIMEOUT,

        // The address specified is already in use and was set to be exclusive.
        ADDRESS_IN_USE,

        // The address specified does not belong to the host.
        ADDRESS_NOT_AVAILABLE
    };

    explicit NetworkChannel(QObject* parent)
        : QObject(parent)
    {
        // Nothing
    }
    virtual ~NetworkChannel() = default;

    int64_t totalRx() const { return total_rx_; }
    int64_t totalTx() const { return total_tx_; }
    int speedRx();
    int speedTx();

    // Converts an error code to a human readable string.
    // Does not support localization. Used for logs.
    static std::string errorToString(ErrorCode error_code);

protected:
    void addTxBytes(size_t bytes_count);
    void addRxBytes(size_t bytes_count);

    static void resizeBuffer(QByteArray* buffer, size_t new_size);

private:
    int64_t total_tx_ = 0;
    int64_t total_rx_ = 0;

    TimePoint begin_time_tx_;
    int64_t bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    int64_t bytes_rx_ = 0;
    int speed_rx_ = 0;
};

} // namespace base

Q_DECLARE_METATYPE(base::NetworkChannel::ErrorCode)

#endif // BASE_NET_NETWORK_CHANNEL_H
