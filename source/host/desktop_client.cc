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

#include "host/desktop_client.h"

#include "base/logging.h"
#include "proto/host_internal.h"

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
quint32 makeUint32(quint16 high, quint16 low)
{
    return (static_cast<quint32>(high) << 16) | static_cast<quint32>(low);
}

//--------------------------------------------------------------------------------------------------
quint16 highWord(quint32 value)
{
    return static_cast<quint16>(value >> 16);
}

//--------------------------------------------------------------------------------------------------
quint16 lowWord(quint32 value)
{
    return static_cast<quint16>(value & 0xFFFF);
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopClient::DesktopClient(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred,
            this, &DesktopClient::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
            this, &DesktopClient::onTcpMessageReceived);
}

//--------------------------------------------------------------------------------------------------
DesktopClient::~DesktopClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::start(const QString& ipc_channel_name)
{
    if (ipc_channel_)
    {
        LOG(ERROR) << "IPC channel already started";
        return;
    }

    if (ipc_channel_name.isEmpty())
    {
        LOG(ERROR) << "Empty name for IPC channel";
        return;
    }

    LOG(INFO) << "Connecting to IPC channel:" << ipc_channel_name;

    if (!connectToAgent(ipc_channel_name))
    {
        emit sig_finished();
        return;
    }

    tcp_channel_->resume();
    ipc_channel_->resume();

    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcChannelChanged(const QString& ipc_channel_name)
{
    if (!connectToAgent(ipc_channel_name))
    {
        emit sig_finished();
        return;
    }

    ipc_channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcChannelMessage(quint32 channel_id, const QByteArray& buffer)
{
    quint16 tcp_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (ipc_channel_id == proto::internal::CHANNEL_ID_SESSION)
    {
        tcp_channel_->send(tcp_channel_id, buffer);
    }
    else
    {
        // TODO: Handle service message.
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onIpcChannelDisconnected()
{
    if (!ipc_channel_)
        return;

    ipc_channel_->disconnect(this); // Disconnect all signals.
    ipc_channel_->deleteLater();
    ipc_channel_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(ERROR) << "TCP error:" << error_code;
    tcp_channel_->disconnect(this); // Disconnect all signals.
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::peer::CHANNEL_ID_SERVICE)
    {
        if (ipc_channel_)
            ipc_channel_->send(makeUint32(proto::internal::CHANNEL_ID_SESSION, channel_id), buffer);
    }
    else
    {
        // TODO: Handle service message.
    }
}

//--------------------------------------------------------------------------------------------------
bool DesktopClient::connectToAgent(const QString& ipc_channel_name)
{
    ipc_channel_ = new base::IpcChannel(this);

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &DesktopClient::onIpcChannelDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &DesktopClient::onIpcChannelMessage);

    if (!ipc_channel_->connectTo(ipc_channel_name))
    {
        LOG(ERROR) << "Unable to connect to IPC server" << ipc_channel_name;
        return false;
    }

    return true;
}

} // namespace host
