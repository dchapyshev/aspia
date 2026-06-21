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

#include "client/android/chat_window.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputMethod>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResizeEvent>
#include <QSaveFile>
#include <QTextStream>
#include <QVBoxLayout>

#include "base/crypto/generic_hash.h"
#include "base/files/base_paths.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "client/client_text_chat.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_state.h"
#include "client/android/chat_view.h"
#include "common/android/app_bar.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"
#include "proto/chat.h"
#include "proto/peer.h"
#include "proto/router_client.h"

namespace {

const int kMaxStoredMessages = 30;
const char kHistoryDirName[] = "chat";

//--------------------------------------------------------------------------------------------------
// A stable identifier for the chat with a host, derived from its address and credentials.
QString chatHistoryId(const SessionState& session_state)
{
    GenericHash hash(GenericHash::SHA1);
    hash.addData(session_state.hostAddress().toUtf8());
    hash.addData(session_state.hostUserName().toUtf8());
    hash.addData(session_state.hostPassword().toUtf8());

    return QString::fromLatin1(hash.result().toHex()).first(32);
}

//--------------------------------------------------------------------------------------------------
QString historyFilePath(const QString& history_id)
{
    const QString base_path = BasePaths::appUserDataDir();
    if (base_path.isEmpty() || history_id.isEmpty())
        return QString();

    QDir dir(base_path);
    if (!dir.mkpath(kHistoryDirName))
        return QString();

    return QDir(dir.filePath(kHistoryDirName)).filePath(history_id + ".json");
}

//--------------------------------------------------------------------------------------------------
// The status line shown for a chat status update, or empty for codes that have no message (typing).
QString statusToString(const proto::chat::Status& status)
{
    const QString user = QString::fromStdString(status.source());

    switch (status.code())
    {
        case proto::chat::Status::CODE_STARTED:
            return QCoreApplication::translate("ChatWindow", "User %1 has joined the chat").arg(user);

        case proto::chat::Status::CODE_STOPPED:
            return QCoreApplication::translate("ChatWindow", "User %1 has left the chat").arg(user);

        case proto::chat::Status::CODE_USER_CONNECTED:
            return QCoreApplication::translate("ChatWindow", "User %1 is logged in").arg(user);

        case proto::chat::Status::CODE_USER_DISCONNECTED:
            return QCoreApplication::translate("ChatWindow", "User %1 is not logged in").arg(user);

        case proto::chat::Status::CODE_NO_USERS:
            return QCoreApplication::translate("ChatWindow", "There are no connected users");

        default:
            return QString();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ChatWindow::ChatWindow(const HostConfig& host, QWidget* parent)
    : QWidget(parent),
      host_(host),
      display_name_(Database::instance().displayName()),
      app_bar_(new AppBar(this)),
      status_(new Label(QString(), Label::Role::CAPTION, this)),
      view_(new ChatView(this))
{
    app_bar_->setTitle(tr("Chat"));
    app_bar_->setBackVisible(true);
    connect(app_bar_, &AppBar::sig_backClicked, this, &ChatWindow::sig_closed);

    IconButton* save_button = new IconButton(":/img/material/save.svg", this);
    IconButton* clear_button = new IconButton(":/img/material/delete.svg", this);
    connect(save_button, &IconButton::clicked, this, &ChatWindow::onSaveChat);
    connect(clear_button, &IconButton::clicked, this, &ChatWindow::onClearChat);
    app_bar_->setActions({ save_button, clear_button });

    status_->setAlignment(Qt::AlignCenter);

    connect(view_, &ChatView::sig_sendText, this, &ChatWindow::onSendText);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(status_);
    layout->addWidget(view_, 1);

    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &ChatWindow::updateKeyboardInset);

    start();
}

//--------------------------------------------------------------------------------------------------
ChatWindow::~ChatWindow() = default;

//--------------------------------------------------------------------------------------------------
void ChatWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateKeyboardInset();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::updateKeyboardInset()
{
    const QInputMethod* input_method = QGuiApplication::inputMethod();

    int inset = 0;
    if (input_method->isVisible())
    {
        // How far the window extends past the top of the keyboard (0 when Android already resized
        // the window to sit above it). The window rect is in logical pixels while the keyboard
        // rectangle is in physical pixels, so scale the latter down.
        const QRect window_rect(mapToGlobal(QPoint(0, 0)), size());
        const int keyboard_top = qRound(input_method->keyboardRectangle().top() / devicePixelRatioF());
        inset = qMax(0, window_rect.bottom() - keyboard_top);
    }

    layout()->setContentsMargins(0, 0, 0, inset);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onStatusChanged(Client::Status status, const QVariant& /* data */)
{
    switch (status)
    {
        case Client::Status::HOST_CONNECTING:
            setStatusText(tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case Client::Status::HOST_CONNECTED:
            status_->setVisible(false);
            view_->setInputEnabled(true);
            break;

        case Client::Status::HOST_DISCONNECTED:
            setStatusText(tr("The connection to the host has been lost."));
            view_->setInputEnabled(false);
            break;

        case Client::Status::NO_ROUTER:
            setStatusText(tr("The specified router is unavailable."));
            break;

        case Client::Status::ROUTER_OFFLINE:
            setStatusText(tr("The specified router is offline."));
            break;

        case Client::Status::ROUTER_ERROR:
            setStatusText(tr("Error requesting connection via router."));
            break;

        case Client::Status::VERSION_MISMATCH:
            setStatusText(tr("The host version is newer than the client. Please update the application."));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onChatMessage(const proto::chat::Chat& chat)
{
    if (chat.has_chat_message())
    {
        const proto::chat::Message& message = chat.chat_message();
        const QString source = QString::fromStdString(message.source());
        const QString text = QString::fromStdString(message.text());

        view_->addMessage(source, text, false, message.timestamp());
        appendHistory({ message.timestamp(), source, text, false, false });
    }
    else if (chat.has_chat_status())
    {
        const QString text = statusToString(chat.chat_status());
        if (!text.isEmpty())
        {
            view_->addStatus(text);
            appendHistory({ 0, QString(), text, false, true });
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onSendText(const QString& text)
{
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    proto::chat::Chat chat;
    proto::chat::Message* message = chat.mutable_chat_message();
    message->set_timestamp(timestamp);
    message->set_source(display_name_.toStdString());
    message->set_text(text.toStdString());

    // Show our own message at once; the host relays it to the other participants, not back to us.
    view_->addMessage(display_name_, text, true, timestamp);
    appendHistory({ timestamp, display_name_, text, true, false });

    emit sig_chatMessage(chat);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::start()
{
    // An empty user name means a connection by ID with a one-time password (#host_id).
    if (host_.username().isEmpty())
        host_.setUsername(u"#" + host_.address());

    session_state_ = std::make_shared<SessionState>(
        host_, proto::peer::SESSION_TYPE_CHAT, Database::instance().displayName());

    history_id_ = chatHistoryId(*session_state_);
    loadHistory();

    setStatusText(tr("Connecting..."));

    if (session_state_->isConnectionByHostId())
        fetchConnectionOffer();
    else
        startNewClient();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::fetchConnectionOffer()
{
    Router* router = Router::instance(session_state_->routerId());
    if (!router)
    {
        setStatusText(tr("The specified router is unavailable."));
        return;
    }

    if (router->status() == Router::Status::ONLINE)
    {
        requestConnectionOffer(router);
        return;
    }

    // The router connection is also dropped while the app is in the background.
    setStatusText(tr("Connecting to router..."));

    disconnect(router, &Router::sig_statusChanged, this, nullptr);
    connect(router, &Router::sig_statusChanged, this,
        [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            return;

        Router* router = Router::instance(session_state_->routerId());
        if (!router)
            return;

        disconnect(router, &Router::sig_statusChanged, this, nullptr);
        requestConnectionOffer(router);
    });

    if (router->status() == Router::Status::OFFLINE)
        router->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::requestConnectionOffer(Router* router)
{
    session_state_->setRouterVersion(router->version());
    setStatusText(tr("Requesting connection to the host..."));

    router->requestConnection(session_state_->hostId(), this,
        [this](const proto::router::ConnectionOffer& offer)
    {
        if (offer.error_code() == proto::router::ConnectionOffer::SUCCESS)
        {
            session_state_->setConnectionOffer(offer);
            startNewClient();
            return;
        }

        setStatusText(tr("Error requesting connection via router."));
    });
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::startNewClient()
{
    ClientChat* client = new ClientChat();

    connect(client, &Client::sig_statusChanged,
            this, &ChatWindow::onStatusChanged, Qt::QueuedConnection);
    connect(client, &ClientChat::sig_chatMessage,
            this, &ChatWindow::onChatMessage, Qt::QueuedConnection);
    connect(this, &ChatWindow::sig_chatMessage,
            client, &ClientChat::onChatMessage, Qt::QueuedConnection);

    client->moveToThread(GuiApplication::ioThread());
    client->setSessionState(session_state_);
    client_ = client;

    QMetaObject::invokeMethod(client, &Client::start, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::setStatusText(const QString& text)
{
    status_->setText(text);
    status_->setVisible(true);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::loadHistory()
{
    const QString file_path = historyFilePath(history_id_);
    if (file_path.isEmpty())
        return;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const QByteArray buffer = file.readAll();
    if (buffer.isEmpty())
        return;

    const QJsonDocument document = QJsonDocument::fromJson(buffer);
    if (!document.isArray())
    {
        LOG(ERROR) << "Invalid chat history format";
        return;
    }

    const QJsonArray items = document.array();
    for (const QJsonValue& value : items)
    {
        if (!value.isObject())
            continue;

        const QJsonObject item = value.toObject();

        HistoryMessage message;
        message.timestamp = static_cast<qint64>(item.value("timestamp").toDouble());
        message.source = item.value("source").toString();
        message.text = item.value("text").toString();
        message.outgoing = item.value("outgoing").toBool();
        message.status = item.value("status").toBool();

        if (message.text.isEmpty())
            continue;

        history_messages_.append(message);

        if (message.status)
            view_->addStatus(message.text);
        else
            view_->addMessage(message.source, message.text, message.outgoing, message.timestamp);
    }

    while (history_messages_.size() > kMaxStoredMessages)
        history_messages_.removeFirst();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::saveHistory() const
{
    const QString file_path = historyFilePath(history_id_);
    if (file_path.isEmpty())
        return;

    if (history_messages_.isEmpty())
    {
        QFile::remove(file_path);
        return;
    }

    QJsonArray items;
    for (const HistoryMessage& message : std::as_const(history_messages_))
    {
        QJsonObject item;
        item["timestamp"] = message.timestamp;
        item["source"] = message.source;
        item["text"] = message.text;
        item["outgoing"] = message.outgoing;
        item["status"] = message.status;
        items.append(item);
    }

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open chat history file for writing:" << file_path;
        return;
    }

    file.write(QJsonDocument(items).toJson(QJsonDocument::Compact));
    file.commit();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::appendHistory(const HistoryMessage& message)
{
    history_messages_.append(message);

    while (history_messages_.size() > kMaxStoredMessages)
        history_messages_.removeFirst();

    saveHistory();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onSaveChat()
{
    if (history_messages_.isEmpty())
        return;

    const QString default_name =
        (host_.name().isEmpty() ? host_.address() : host_.name()) + u".txt";
    const QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save Chat"), default_name, tr("Text files (*.txt)"));
    if (file_path.isEmpty())
        return;

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        MessageDialog::info(this, tr("Chat"), tr("Could not open the file for writing."));
        return;
    }

    QTextStream stream(&file);
    for (const HistoryMessage& message : std::as_const(history_messages_))
    {
        if (message.status)
        {
            stream << message.text << Qt::endl;
        }
        else
        {
            const QString time =
                QDateTime::fromSecsSinceEpoch(message.timestamp).toString("yyyy-MM-dd HH:mm");
            stream << "[" << time << "] " << message.source << ": " << message.text << Qt::endl;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onClearChat()
{
    if (!MessageDialog::confirm(this, tr("Clear Chat"), tr("Clear the chat history?"), tr("Clear")))
        return;

    view_->clear();
    history_messages_.clear();
    saveHistory();
}
