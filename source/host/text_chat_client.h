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

#ifndef HOST_TEXT_CHAT_CLIENT_H
#define HOST_TEXT_CHAT_CLIENT_H

#include <QObject>

#include "base/net/tcp_channel.h"
#include "proto/text_chat.h"

namespace host {

class TextChatClient final : public QObject
{
    Q_OBJECT

public:
    explicit TextChatClient(base::TcpChannel* tcp_channel, QObject* parent = nullptr);
    ~TextChatClient() final;

    quint32 clientId() const;
    QString displayName() const;
    QString computerName() const;

public slots:
    void start();
    void onSendTextChat(const proto::text_chat::TextChat& text_chat);
    void onSendStatus(proto::text_chat::Status::Code code);

signals:
    void sig_started(quint32 client_id);
    void sig_finished(quint32 client_id);
    void sig_messageReceived(quint32 client_id, const proto::text_chat::TextChat& text_chat);

private slots:
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 tcp_channel_id, const QByteArray& buffer);

    void onUserSessionEvent(quint32 event_type, quint32 session_id);

private:
    base::TcpChannel* tcp_channel_ = nullptr;
    bool has_user_ = true;

    Q_DISABLE_COPY_MOVE(TextChatClient)
};

} // namespace host

#endif // HOST_TEXT_CHAT_CLIENT_H
