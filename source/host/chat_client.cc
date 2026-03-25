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

#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ChatClient::ChatClient(base::TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, FEATURE_NONE, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ChatClient::~ChatClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onSendChat(const proto::chat::Chat& chat)
{
    send(proto::peer::CHANNEL_ID_0, base::serialize(chat));
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onSendStatus(proto::chat::Status_Code code)
{
    LOG(INFO) << "Send text chat status:" << code;

    proto::chat::Chat text_chat;
    proto::chat::Status* text_chat_status = text_chat.mutable_chat_status();

    text_chat_status->set_code(code);
    text_chat_status->set_source(base::SysInfo::computerName().toStdString());

    onSendChat(text_chat);
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onStart()
{
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void ChatClient::onMessage(quint8 channel_id, const QByteArray& buffer)
{
    proto::chat::Chat chat;
    if (!base::parse(buffer, &chat))
    {
        LOG(ERROR) << "Unable to parse chat message";
        return;
    }

    emit sig_messageReceived(chat);
}

} // namespace host
