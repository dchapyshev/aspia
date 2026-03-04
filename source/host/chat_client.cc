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

#include "host/chat_client.h"

#include <QVariant>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ChatClient::ChatClient(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    setProperty("client_id", tcp_channel_->instanceId());
    setProperty("version", tcp_channel_->peerVersion().toString());
    setProperty("os_name", tcp_channel_->peerOsName());
    setProperty("session_type", tcp_channel_->peerSessionType());
    setProperty("user_name", tcp_channel_->peerUserName());
    setProperty("display_name", tcp_channel_->peerDisplayName());
    setProperty("computer_name", tcp_channel_->peerComputerName());
    setProperty("arch", tcp_channel_->peerArchitecture());

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &ChatClient::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &ChatClient::onTcpMessageReceived);
}

//--------------------------------------------------------------------------------------------------
ChatClient::~ChatClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ChatClient::start()
{
    tcp_channel_->resume();
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onSendChat(const proto::chat::Chat& chat)
{
    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, base::serialize(chat));
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onSendStatus(proto::chat::Status::Code code)
{
    LOG(INFO) << "Send text chat status:" << code;

    switch (code)
    {
        case proto::chat::Status::CODE_USER_CONNECTED:
            has_user_ = true;
            break;

        case proto::chat::Status::CODE_USER_DISCONNECTED:
            has_user_ = false;
            break;

        default:
            break;
    }

    proto::chat::Chat text_chat;
    proto::chat::Status* text_chat_status = text_chat.mutable_chat_status();

    text_chat_status->set_code(code);
    text_chat_status->set_source(base::SysInfo::computerName().toStdString());

    onSendChat(text_chat);
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(WARNING) << "TCP error occurred:" << error_code;
    tcp_channel_->disconnect(this);
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    proto::chat::Chat chat;
    if (!base::parse(buffer, &chat))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    if (!has_user_)
    {
        if (chat.has_chat_message())
            onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);
        return;
    }

    emit sig_messageReceived(chat);
}

} // namespace host
