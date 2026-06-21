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

#ifndef CLIENT_ANDROID_CHAT_WINDOW_H
#define CLIENT_ANDROID_CHAT_WINDOW_H

#include <QList>
#include <QWidget>

#include <memory>

#include "base/scoped_qpointer.h"
#include "client/client.h"
#include "client/config.h"

namespace proto::chat {
class Chat;
} // namespace proto::chat

class AppBar;
class ChatView;
class ClientChat;
class IconButton;
class Label;
class Router;
class SessionState;

class ChatWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const HostConfig& host, QWidget* parent = nullptr);
    ~ChatWindow() final;

signals:
    void sig_closed();

    // Routed to the client (queued).
    void sig_chatMessage(const proto::chat::Chat& chat);

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onStatusChanged(Client::Status status, const QVariant& data);
    void onChatMessage(const proto::chat::Chat& chat);

    // Lifts the content by however much the on-screen keyboard overlaps the window. Android's
    // adjustResize is unreliable here (it sometimes leaves the window full height with the keyboard
    // up), so the keyboard rectangle reported by Qt is the source of truth.
    void updateKeyboardInset();

    void onSaveChat();
    void onClearChat();

private:
    // A persisted chat entry: a message, or (when |status| is set) a status line.
    struct HistoryMessage
    {
        qint64 timestamp;
        QString source;
        QString text;
        bool outgoing;
        bool status;
    };

    void start();
    void fetchConnectionOffer();
    void requestConnectionOffer(Router* router);
    void startNewClient();
    void onSendText(const QString& text);
    void setStatusText(const QString& text);

    void loadHistory();
    void saveHistory() const;
    void appendHistory(const HistoryMessage& message);

    HostConfig host_;
    QString display_name_;
    QString history_id_;
    QList<HistoryMessage> history_messages_;
    std::shared_ptr<SessionState> session_state_;
    ScopedQPointer<ClientChat> client_;

    AppBar* app_bar_ = nullptr;
    Label* status_ = nullptr;
    ChatView* view_ = nullptr;

    Q_DISABLE_COPY_MOVE(ChatWindow)
};

#endif // CLIENT_ANDROID_CHAT_WINDOW_H
