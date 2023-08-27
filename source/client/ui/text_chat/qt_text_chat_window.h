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

#ifndef CLIENT_UI_TEXT_CHAT_QT_TEXT_CHAT_WINDOW_H
#define CLIENT_UI_TEXT_CHAT_QT_TEXT_CHAT_WINDOW_H

#include "base/macros_magic.h"
#include "client/text_chat_window_proxy.h"
#include "client/ui/session_window.h"
#include "proto/text_chat.pb.h"

#include <QTreeWidget>

namespace Ui {
class TextChatWindow;
} // namespace Ui

class QHBoxLayout;

namespace client {

class QtTextChatWindow
    : public SessionWindow,
      public TextChatWindow
{
    Q_OBJECT

public:
    explicit QtTextChatWindow(QWidget* parent = nullptr);
    ~QtTextChatWindow() override;

    // SessionWindow implementation.
    std::unique_ptr<Client> createClient() override;

    // TextChatWindow implementation.
    void start(std::shared_ptr<TextChatControlProxy> text_chat_control_proxy) override;
    void onTextChatMessage(const proto::TextChat& text_chat) override;

signals:
    void sig_textChatMessage(const proto::TextChat& text_chat);

private:
    std::unique_ptr<Ui::TextChatWindow> ui;

    std::shared_ptr<TextChatControlProxy> text_chat_control_proxy_;
    std::shared_ptr<TextChatWindowProxy> text_chat_window_proxy_;

    DISALLOW_COPY_AND_ASSIGN(QtTextChatWindow);
};

} // namespace client

#endif // CLIENT_UI_TEXT_CHAT_QT_TEXT_CHAT_WINDOW_H
