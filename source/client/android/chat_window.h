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
#include <QPointer>
#include <QWidget>

#include <memory>

#include "client/config.h"
#include "client/workers/network_worker.h"

namespace proto::chat {
class Chat;
} // namespace proto::chat

class AppBar;
class ChatView;
class IconButton;
class QTimer;
class Router;
class SessionKeeper;
class SessionState;
class WorkerManager;

class ChatWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const HostConfig& host, QWidget* parent = nullptr);
    ~ChatWindow() final;

signals:
    void sig_closed();
    void sig_startConnection(std::shared_ptr<SessionState> session_state);
    void sig_sessionReady();
    void sig_sendMessage(quint8 channel_id, const QByteArray& buffer);

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onNetworkConnected();
    void onChannelMessage(const QByteArray& buffer);
    void onStatusChanged(NetworkWorker::Status status, const QVariant& data);
    void onChatMessage(const proto::chat::Chat& chat);

    // Lifts the content by however much the on-screen keyboard overlaps the window. Android's
    // adjustResize is unreliable here (it sometimes leaves the window full height with the keyboard
    // up), so the keyboard rectangle reported by Qt is the source of truth.
    void updateKeyboardInset();

    void onSaveChat();
    void onClearChat();

    // Throttled send of our own typing status; clears the incoming "is typing" line on timeout.
    void onTyping();
    void clearTypingStatus();

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
    void startNewSession();
    void sendChatMessage(const proto::chat::Chat& chat);
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

    std::unique_ptr<WorkerManager> worker_manager_;
    QPointer<NetworkWorker> network_worker_;
    SessionKeeper* session_keeper_ = nullptr;

    AppBar* app_bar_ = nullptr;
    ChatView* view_ = nullptr;
    QTimer* typing_throttle_ = nullptr;
    QTimer* typing_clear_timer_ = nullptr;

    Q_DISABLE_COPY_MOVE(ChatWindow)
};

#endif // CLIENT_ANDROID_CHAT_WINDOW_H
