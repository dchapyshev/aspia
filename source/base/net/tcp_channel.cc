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
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(bytes))) +
        ((1.0 - kAlpha) * static_cast<double>(last_speed)));
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
TcpChannel::TcpChannel(QObject* parent)
    : QObject(parent),
      instance_id_(makeInstanceId())
{
    // Nothing
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
