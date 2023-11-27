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

#include "client/ui/text_chat/qt_text_chat_window.h"

#include "base/logging.h"
#include "client/client_text_chat.h"
#include "client/text_chat_control_proxy.h"
#include "qt_base/application.h"
#include "ui_qt_text_chat_window.h"

namespace client {

//--------------------------------------------------------------------------------------------------
QtTextChatWindow::QtTextChatWindow(QWidget* parent)
    : SessionWindow(parent),
      ui(std::make_unique<Ui::TextChatWindow>()),
      text_chat_window_proxy_(
          std::make_shared<TextChatWindowProxy>(qt_base::Application::uiTaskRunner(), this))
{
    LOG(LS_INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendMessage,
            this, [this](const proto::TextChatMessage& message)
    {
        if (text_chat_control_proxy_)
        {
            proto::TextChat text_chat;
            text_chat.mutable_chat_message()->CopyFrom(message);
            text_chat_control_proxy_->onTextChatMessage(text_chat);
        }
        else
        {
            LOG(LS_ERROR) << "Invalid text chat proxy";
        }
    });

    connect(ui->text_chat_widget, &common::TextChatWidget::sig_sendStatus,
            this, [this](const proto::TextChatStatus& status)
    {
        if (text_chat_control_proxy_)
        {
            proto::TextChat text_chat;
            text_chat.mutable_chat_status()->CopyFrom(status);
            text_chat_control_proxy_->onTextChatMessage(text_chat);
        }
        else
        {
            LOG(LS_ERROR) << "Invalid text chat proxy";
        }
    });
}

//--------------------------------------------------------------------------------------------------
QtTextChatWindow::~QtTextChatWindow()
{
    LOG(LS_INFO) << "Dtor";
    text_chat_window_proxy_->dettach();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Client> QtTextChatWindow::createClient()
{
    std::unique_ptr<ClientTextChat> client = std::make_unique<ClientTextChat>(
        qt_base::Application::ioTaskRunner());

    client->setTextChatWindow(text_chat_window_proxy_);

    return std::move(client);
}

//--------------------------------------------------------------------------------------------------
void QtTextChatWindow::start(std::shared_ptr<TextChatControlProxy> text_chat_control_proxy)
{
    text_chat_control_proxy_ = std::move(text_chat_control_proxy);
    DCHECK(text_chat_control_proxy_);

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
            activateWindow();
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

} // namespace client
