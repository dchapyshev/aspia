//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_UI_TEXT_CHAT_WIDGET_H
#define COMMON_UI_TEXT_CHAT_WIDGET_H

#include "proto/text_chat.h"

#include <QWidget>

namespace Ui {
class TextChatWidget;
} // namespace Ui

namespace common {

class TextChatWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TextChatWidget(QWidget* parent = nullptr);
    ~TextChatWidget() final;

    void readMessage(const proto::text_chat::Message& message);
    void readStatus(const proto::text_chat::Status& status);

    void setDisplayName(const QString& display_name);

signals:
    void sig_sendMessage(const proto::text_chat::Message& message);
    void sig_sendStatus(const proto::text_chat::Status& status);
    void sig_textChatClosed();

protected:
    bool eventFilter(QObject* object, QEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;
    void closeEvent(QCloseEvent* event) final;

private:
    void addOutgoingMessage(time_t timestamp, const QString& message);
    void addStatusMessage(const QString& message);
    void onSendMessage();
    void onSendStatus(proto::text_chat::Status::Code code);
    void onClearHistory();
    void onSaveChat();
    void onUpdateSize();

    std::unique_ptr<Ui::TextChatWidget> ui;
    QString display_name_;
    QTimer* status_clear_timer_;
};

} // namespace common

#endif // COMMON_UI_TEXT_CHAT_WIDGET_H
