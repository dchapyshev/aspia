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

#include "client/ui/text_chat/qt_text_chat_window.h"

#include "base/logging.h"
#include "client/client_text_chat.h"
#include "qt_base/application.h"
#include "ui_qt_text_chat_window.h"

Q_DECLARE_METATYPE(proto::TextChat)

namespace client {

//--------------------------------------------------------------------------------------------------
QtTextChatWindow::QtTextChatWindow(QWidget* parent)
    : SessionWindow(nullptr, parent),
      ui(std::make_unique<Ui::TextChatWindow>())
{
    LOG(LS_INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendMessage,
            this, [this](const proto::TextChatMessage& message)
    {
        proto::TextChat text_chat;
        text_chat.mutable_chat_message()->CopyFrom(message);
        emit sig_textChatMessage(text_chat);
    });

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendStatus,
            this, [this](const proto::TextChatStatus& status)
    {
        proto::TextChat text_chat;
        text_chat.mutable_chat_status()->CopyFrom(status);
        emit sig_textChatMessage(text_chat);
    });
}

//--------------------------------------------------------------------------------------------------
QtTextChatWindow::~QtTextChatWindow()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Client> QtTextChatWindow::createClient()
{
    LOG(LS_INFO) << "Create client";

    std::unique_ptr<ClientTextChat> client = std::make_unique<ClientTextChat>(
        base::GuiApplication::ioTaskRunner());

    connect(this, &QtTextChatWindow::sig_textChatMessage,
            client.get(), &ClientTextChat::onTextChatMessage,
            Qt::QueuedConnection);
    connect(client.get(), &ClientTextChat::sig_start,
            this, &QtTextChatWindow::start,
            Qt::QueuedConnection);
    connect(client.get(), &ClientTextChat::sig_textChatMessage,
            this, &QtTextChatWindow::onTextChatMessage,
            Qt::QueuedConnection);

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void QtTextChatWindow::start()
{
    LOG(LS_INFO) << "Show window";

    ui->text_chat_widget->setDisplayName(sessionState()->displayName());

    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void QtTextChatWindow::onTextChatMessage(const proto::TextChat& text_chat)
{
    if (text_chat.has_chat_message())
    {
        ui->text_chat_widget->readMessage(text_chat.chat_message());

        if (QApplication::applicationState() != Qt::ApplicationActive)
        {
            LOG(LS_INFO) << "Activate text chat window";
            activateWindow();
        }
    }
    else if (text_chat.has_chat_status())
    {
        ui->text_chat_widget->readStatus(text_chat.chat_status());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled text chat message";
    }
}

//--------------------------------------------------------------------------------------------------
void QtTextChatWindow::onInternalReset()
{
    // Nothing
}

} // namespace client
