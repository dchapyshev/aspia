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

#ifndef CLIENT_UI_TEXT_CHAT_TEXT_CHAT_SESSION_WINDOW_H
#define CLIENT_UI_TEXT_CHAT_TEXT_CHAT_SESSION_WINDOW_H

#include <QTreeWidget>

#include "client/ui/session_window.h"
#include "proto/text_chat.h"

namespace Ui {
class TextChatSessionWindow;
} // namespace Ui

class QHBoxLayout;

namespace client {

class TextChatSessionWindow final : public SessionWindow
{
    Q_OBJECT

public:
    explicit TextChatSessionWindow(QWidget* parent = nullptr);
    ~TextChatSessionWindow() final;

    // SessionWindow implementation.
    Client* createClient() final;

public slots:
    void onShowWindow();
    void onTextChatMessage(const proto::text_chat::TextChat& text_chat);

signals:
    void sig_textChatMessage(const proto::text_chat::TextChat& text_chat);

protected:
    // SessionWindow implementation.
    void onInternalReset() final;

private:
    std::unique_ptr<Ui::TextChatSessionWindow> ui;
    Q_DISABLE_COPY(TextChatSessionWindow)
};

} // namespace client

#endif // CLIENT_UI_TEXT_CHAT_TEXT_CHAT_SESSION_WINDOW_H
