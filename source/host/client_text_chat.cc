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

#include "host/client_text_chat.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "proto/text_chat.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ClientTextChat::ClientTextChat(base::TcpChannel* channel, QObject* parent)
    : Client(channel, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ClientTextChat::~ClientTextChat()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::sendTextChat(const proto::text_chat::TextChat& text_chat)
{
    sendMessage(base::serialize(text_chat));
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::sendStatus(proto::text_chat::Status::Code code)
{
    LOG(INFO) << "Send text chat status";

    proto::text_chat::TextChat text_chat;
    proto::text_chat::Status* text_chat_status = text_chat.mutable_chat_status();

    text_chat_status->set_code(code);
    text_chat_status->set_source(base::SysInfo::computerName().toStdString());

    sendTextChat(text_chat);
}

//--------------------------------------------------------------------------------------------------
bool ClientTextChat::hasUser() const
{
    return has_user_;
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::setHasUser(bool enable)
{
    LOG(INFO) << "Has user changed (has_user=" << enable << ")";
    has_user_ = enable;
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onStarted()
{
    LOG(INFO) << "Session started";
}

//--------------------------------------------------------------------------------------------------
void ClientTextChat::onReceived(const QByteArray& buffer)
{
    proto::text_chat::TextChat text_chat;

    if (!base::parse(buffer, &text_chat))
    {
        LOG(ERROR) << "Unable to parse system info request";
        return;
    }

    if (hasUser())
    {
        emit sig_clientTextChat(clientId(), text_chat);
    }
    else
    {
        if (text_chat.has_chat_message())
            sendStatus(proto::text_chat::Status::CODE_USER_DISCONNECTED);
    }
}

} // namespace host
