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

#include "client/ui/chat/chat_session_window.h"

#include "base/logging.h"
#include "client/client_text_chat.h"
#include "ui_chat_session_window.h"

namespace client {

//--------------------------------------------------------------------------------------------------
ChatSessionWindow::ChatSessionWindow(QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::ChatSessionWindow>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->text_chat_widget, &common::ChatWidget::sig_sendMessage,
            this, [this](const proto::chat::Message& message)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_message()->CopyFrom(message);
        emit sig_chatMessage(chat);
    });

    connect(ui->text_chat_widget, &common::ChatWidget::sig_sendStatus,
            this, [this](const proto::chat::Status& status)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_status()->CopyFrom(status);
        emit sig_chatMessage(chat);
    });
}

//--------------------------------------------------------------------------------------------------
ChatSessionWindow::~ChatSessionWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* ChatSessionWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientChat* client = new ClientChat();

    connect(this, &ChatSessionWindow::sig_chatMessage, client, &ClientChat::onChatMessage,
            Qt::QueuedConnection);
    connect(client, &ClientChat::sig_showSessionWindow, this, &ChatSessionWindow::onShowWindow,
            Qt::QueuedConnection);
    connect(client, &ClientChat::sig_chatMessage, this, &ChatSessionWindow::onChatMessage,
            Qt::QueuedConnection);

    return client;
}

//--------------------------------------------------------------------------------------------------
void ChatSessionWindow::onShowWindow()
{
    LOG(INFO) << "Show window";

    ui->text_chat_widget->setDisplayName(sessionState()->displayName());

    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void ChatSessionWindow::onChatMessage(const proto::chat::Chat& chat)
{
    if (chat.has_chat_message())
    {
        ui->text_chat_widget->readMessage(chat.chat_message());

        if (QApplication::applicationState() != Qt::ApplicationActive)
        {
            LOG(INFO) << "Activate text chat window";
            activateWindow();
        }
    }
    else if (chat.has_chat_status())
    {
        ui->text_chat_widget->readStatus(chat.chat_status());
    }
    else
    {
        LOG(ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void ChatSessionWindow::onInternalReset()
{
    // Nothing
}

} // namespace client
