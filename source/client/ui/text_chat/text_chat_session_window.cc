//
// Aspia Project
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

#include "client/ui/text_chat/text_chat_session_window.h"

#include "base/logging.h"
#include "client/client_text_chat.h"
#include "ui_text_chat_session_window.h"

namespace client {

//--------------------------------------------------------------------------------------------------
TextChatSessionWindow::TextChatSessionWindow(QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::TextChatSessionWindow>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendMessage,
            this, [this](const proto::text_chat::Message& message)
    {
        proto::text_chat::TextChat text_chat;
        text_chat.mutable_chat_message()->CopyFrom(message);
        emit sig_textChatMessage(text_chat);
    });

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendStatus,
            this, [this](const proto::text_chat::Status& status)
    {
        proto::text_chat::TextChat text_chat;
        text_chat.mutable_chat_status()->CopyFrom(status);
        emit sig_textChatMessage(text_chat);
    });
}

//--------------------------------------------------------------------------------------------------
TextChatSessionWindow::~TextChatSessionWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* TextChatSessionWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientTextChat* client = new ClientTextChat();

    connect(this, &TextChatSessionWindow::sig_textChatMessage, client, &ClientTextChat::onTextChatMessage,
            Qt::QueuedConnection);
    connect(client, &ClientTextChat::sig_showSessionWindow, this, &TextChatSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientTextChat::sig_textChatMessage, this, &TextChatSessionWindow::onTextChatMessage,
            Qt::QueuedConnection);

    return client;
}

//--------------------------------------------------------------------------------------------------
void TextChatSessionWindow::onShowWindow()
{
    LOG(INFO) << "Show window";

    ui->text_chat_widget->setDisplayName(sessionState()->displayName());

    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void TextChatSessionWindow::onTextChatMessage(const proto::text_chat::TextChat& text_chat)
{
    if (text_chat.has_chat_message())
    {
        ui->text_chat_widget->readMessage(text_chat.chat_message());

        if (QApplication::applicationState() != Qt::ApplicationActive)
        {
            LOG(INFO) << "Activate text chat window";
            activateWindow();
        }
    }
    else if (text_chat.has_chat_status())
    {
        ui->text_chat_widget->readStatus(text_chat.chat_status());
    }
    else
    {
        LOG(ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void TextChatSessionWindow::onInternalReset()
{
    // Nothing
}

} // namespace client
