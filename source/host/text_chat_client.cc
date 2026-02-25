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

#include "host/text_chat_client.h"

#include "base/application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"

namespace host {

//--------------------------------------------------------------------------------------------------
TextChatClient::TextChatClient(base::TcpChannel* tcp_channel, QObject* parent)
    : QObject(parent),
      tcp_channel_(tcp_channel)
{
    LOG(INFO) << "Ctor";
    CHECK(tcp_channel_);

    tcp_channel_->setParent(this);

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred,
            this, &TextChatClient::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
            this, &TextChatClient::onTcpMessageReceived);

    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &TextChatClient::onUserSessionEvent);
}

//--------------------------------------------------------------------------------------------------
TextChatClient::~TextChatClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
quint32 TextChatClient::clientId() const
{
    return tcp_channel_->instanceId();
}

//--------------------------------------------------------------------------------------------------
QString TextChatClient::displayName() const
{
    return tcp_channel_->peerDisplayName();
}

//--------------------------------------------------------------------------------------------------
QString TextChatClient::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::start()
{
    tcp_channel_->resume();
    emit sig_started(clientId());
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::onSendTextChat(const proto::text_chat::TextChat& text_chat)
{
    tcp_channel_->send(proto::peer::CHANNEL_ID_SESSION, base::serialize(text_chat));
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::onSendStatus(proto::text_chat::Status::Code code)
{
    LOG(INFO) << "Send text chat status:" << code;

    switch (code)
    {
        case proto::text_chat::Status::CODE_USER_CONNECTED:
            has_user_ = true;
            break;

        case proto::text_chat::Status::CODE_USER_DISCONNECTED:
            has_user_ = false;
            break;

        default:
            break;
    }

    proto::text_chat::TextChat text_chat;
    proto::text_chat::Status* text_chat_status = text_chat.mutable_chat_status();

    text_chat_status->set_code(code);
    text_chat_status->set_source(base::SysInfo::computerName().toStdString());

    onSendTextChat(text_chat);
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(WARNING) << "TCP error occurred:" << error_code;

    tcp_channel_->disconnect(this);
    emit sig_finished(clientId());
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer)
{
    proto::text_chat::TextChat text_chat;

    if (!base::parse(buffer, &text_chat))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    if (has_user_)
    {
        emit sig_messageReceived(clientId(), text_chat);
    }
    else
    {
        if (text_chat.has_chat_message())
            onSendStatus(proto::text_chat::Status::CODE_USER_DISCONNECTED);
    }
}

//--------------------------------------------------------------------------------------------------
void TextChatClient::onUserSessionEvent(quint32 event_type, quint32 session_id)
{

}

} // namespace host
