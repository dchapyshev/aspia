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

#ifndef COMMON_DESKTOP_CHAT_WIDGET_H
#define COMMON_DESKTOP_CHAT_WIDGET_H

#include <QShowEvent>
#include <QVector>
#include <QWidget>

#include <memory>

#include "proto/chat.h"

namespace Ui {
class ChatWidget;
} // namespace Ui

class QAction;

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
    void setChatEnabled(bool enable);
    void setToolsButtonVisible(bool visible);

    QList<QAction*> tabActions() const;

signals:
    void sig_sendMessage(const proto::chat::Message& message);
    void sig_sendStatus(const proto::chat::Status& status);
    void sig_textChatClosed();

protected:
    // QWidget implementation.
    bool eventFilter(QObject* object, QEvent* event) final;
    void changeEvent(QEvent* event) final;
    void closeEvent(QCloseEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void showEvent(QShowEvent* event) final;

private slots:
    void onMessageTextChanged();
    void onClearHistory();
    void onSaveChat();
    void onSendMessage();
    void onUpdateSize();

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
    void sendStatus(proto::chat::Status::Code code);
    void refreshStyles();

    std::unique_ptr<Ui::ChatWidget> ui;
    QAction* action_save_chat_;
    QAction* action_clear_chat_;
    QString display_name_;
    QString history_id_;
    QVector<HistoryMessage> history_messages_;
    QTimer* status_clear_timer_;
};

#endif // COMMON_DESKTOP_CHAT_WIDGET_H
