//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "proto/text_chat.pb.h"

#include <QWidget>

namespace Ui {
class TextChatWidget;
} // namespace Ui

namespace common {

class TextChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextChatWidget(QWidget* parent = nullptr);
    ~TextChatWidget() override;

    void readMessage(const proto::TextChatMessage& message);
    void readStatus(const proto::TextChatStatus& status);

    void setDisplayName(const std::string& display_name);

signals:
    void sig_sendMessage(const proto::TextChatMessage& message);
    void sig_sendStatus(const proto::TextChatStatus& status);
    void sig_textChatClosed();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void addOutgoingMessage(time_t timestamp, const QString& message);
    void addStatusMessage(const QString& message);
    void onSendMessage();
    void onSendStatus(proto::TextChatStatus::Status status);
    void onClearHistory();
    void onSaveChat();
    void onUpdateSize();

    std::unique_ptr<Ui::TextChatWidget> ui;
    std::string display_name_;
    QTimer* status_clear_timer_;
};

} // namespace common

#endif // COMMON_UI_TEXT_CHAT_WIDGET_H
