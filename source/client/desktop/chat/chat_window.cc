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

#include "client/desktop/chat/chat_window.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "client/workers/network_worker.h"
#include "common/desktop/chat_widget.h"
#include "proto/chat.h"
#include "proto/peer.h"
#include "ui_chat_window.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString chatHistoryId(const SessionState& session_state)
{
    GenericHash hash(GenericHash::SHA1);
    hash.addData(session_state.hostAddress().toUtf8());
    hash.addData(session_state.hostUserName().toUtf8());
    hash.addData(session_state.hostPassword().toUtf8());

    return QString::fromLatin1(hash.result().toHex()).first(32);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ChatWindow::ChatWindow(QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_CHAT, parent),
      ui(std::make_unique<Ui::ChatWindow>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->text_chat_widget, &ChatWidget::sig_sendMessage,
            this, [this](const proto::chat::Message& message)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_message()->CopyFrom(message);
        sendMessage(proto::peer::CHANNEL_ID_0, serialize(chat));
    });

    connect(ui->text_chat_widget, &ChatWidget::sig_sendStatus,
            this, [this](const proto::chat::Status& status)
    {
        proto::chat::Chat chat;
        chat.mutable_chat_status()->CopyFrom(status);
        sendMessage(proto::peer::CHANNEL_ID_0, serialize(chat));
    });
}

//--------------------------------------------------------------------------------------------------
ChatWindow::~ChatWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::setTabbedMode(bool tabbed)
{
    LOG(INFO) << "Set tabbed mode:" << tabbed;
    ui->text_chat_widget->setToolsButtonVisible(!tabbed);
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> ChatWindow::tabActionGroups() const
{
    return {
        { Tab::ActionRole::FILE, ui->text_chat_widget->tabActions() },
        { Tab::ActionRole::ACTION, sessionConnectActions() }
    };
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onInternalReset()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onRegisterWorkers()
{
    ui->text_chat_widget->setHistoryId(chatHistoryId(*sessionState()));

    connect(networkWorker(), &NetworkWorker::sig_channel_0,
            this, &ChatWindow::onChannelMessage, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onSessionStarted()
{
    LOG(INFO) << "Chat session started";

    ui->text_chat_widget->setDisplayName(sessionState()->displayName());

    show();
    activateWindow();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onChannelMessage(const QByteArray& buffer)
{
    proto::chat::Chat chat;
    if (!parse(buffer, &chat))
    {
        LOG(ERROR) << "Unable to parse text chat message";
        return;
    }

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
