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

#include "base/net/udp_channel.h"

#include <asio/connect.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QString addressToString(const asio::ip::address& address)
{
    return QString::fromLocal8Bit(address.to_string());
}

//--------------------------------------------------------------------------------------------------
QString endpointToString(const asio::ip::udp::endpoint& endpoint)
{
    return addressToString(endpoint.address()) + ':' + QString::number(endpoint.port());
}

//--------------------------------------------------------------------------------------------------
QStringList endpointsToString(const asio::ip::udp::resolver::results_type& endpoints)
{
    if (endpoints.empty())
        return QStringList();

    QStringList list;
    list.resize(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(endpointToString(it->endpoint()));

    return list;
}

} // namespace

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(AsioEventDispatcher::ioContext())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UdpChannel::UdpChannel(asio::ip::udp::socket&& socket, QObject* parent)
    : QObject(parent),
      resolver_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UdpChannel::~UdpChannel()
{
    resolver_.cancel();

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::connectTo(const QString& address, quint16 port)
{
    resolver_.async_resolve(address.toLocal8Bit().toStdString(), std::to_string(port),
        [&](const std::error_code& error_code, const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [&](const std::error_code& error_code, const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                    onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(INFO) << "Connected to" << endpointToString(endpoint);
            onConnected();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::send(quint8 channel_id, const QByteArray& buffer)
{
    // TODO: Send
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onConnected()
{
    // TODO: Read
}

//--------------------------------------------------------------------------------------------------
void UdpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    if (error_code)
        LOG(ERROR) << "Error occurred" << error_code << "from" << location.toString();
    else
        LOG(ERROR) << "Error occurred from" << location.toString();

    emit sig_errorOccurred();
}

} // namespace base
