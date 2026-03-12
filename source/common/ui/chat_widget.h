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

#ifndef COMMON_UI_CHAT_WIDGET_H
#define COMMON_UI_CHAT_WIDGET_H

#include "proto/chat.h"

#include <QVector>
#include <QWidget>

namespace Ui {
class ChatWidget;
} // namespace Ui

namespace common {

class ChatWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget* parent = nullptr);
    ~ChatWidget() final;

    void readMessage(const proto::chat::Message& message);
    void readStatus(const proto::chat::Status& status);

    void setDisplayName(const QString& display_name);
    void setHistoryId(const QString& history_id);

signals:
    void sig_sendMessage(const proto::chat::Message& message);
    void sig_sendStatus(const proto::chat::Status& status);
    void sig_textChatClosed();

protected:
    bool eventFilter(QObject* object, QEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private:
    struct HistoryMessage
    {
        qint64 timestamp = 0;
        QString source;
        QString text;
        bool outgoing = false;
        bool status = false;
    };

    void addIncomingMessage(time_t timestamp, const QString& source, const QString& message);
    void addOutgoingMessage(time_t timestamp, const QString& message);
    void addStatusMessage(const QString& message);
    void appendHistoryMessage(qint64 timestamp, const QString& source, const QString& text, bool outgoing);
    void appendHistoryStatus(const QString& text);
    void loadHistory();
    void saveHistory() const;
    void clearMessages();
    void onSendMessage();
    void onSendStatus(proto::chat::Status::Code code);
    void onClearHistory();
    void onSaveChat();
    void onUpdateSize();

    std::unique_ptr<Ui::ChatWidget> ui;
    QString display_name_;
    QString history_id_;
    QVector<HistoryMessage> history_messages_;
    QTimer* status_clear_timer_;
};

} // namespace common

#endif // COMMON_UI_CHAT_WIDGET_H
