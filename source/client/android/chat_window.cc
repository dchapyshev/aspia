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
#include <QTimer>
#include <QVBoxLayout>

#include "base/crypto/generic_hash.h"
#include "base/files/base_paths.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "base/threading/worker_manager.h"
#include "client/database.h"
#include "client/router.h"
#include "client/session_keeper.h"
#include "client/session_state.h"
#include "client/workers/network_worker.h"
#include "client/android/chat_view.h"
#include "common/android/app_bar.h"
#include "common/android/icon_button.h"
#include "common/android/message_dialog.h"
#include "proto/chat.h"
#include "proto/peer.h"
#include "proto/router_client.h"

namespace {

const int kMaxStoredMessages = 30;
const char kHistoryDirName[] = "chat";

// How often our own typing status is re-sent while the user keeps typing.
const int kTypingThrottleMs = 3000;

// How long an incoming "is typing" line stays before it is cleared.
const int kTypingClearMs = 5000;

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
      view_(new ChatView(this)),
      typing_throttle_(new QTimer(this)),
      typing_clear_timer_(new QTimer(this))
{
    // Fall back to the device name when no display name is configured, so our messages and typing
    // status carry an identifiable source on the other end.
    if (display_name_.isEmpty())
        display_name_ = SysInfo::computerName();

    typing_throttle_->setSingleShot(true);
    typing_clear_timer_->setSingleShot(true);
    connect(typing_clear_timer_, &QTimer::timeout, this, &ChatWindow::clearTypingStatus);
    app_bar_->setTitle(tr("Chat"));
    app_bar_->setBackVisible(true);
    connect(app_bar_, &AppBar::sig_backClicked, this, &ChatWindow::sig_closed);

    IconButton* save_button = new IconButton(":/img/material/save.svg", this);
    IconButton* clear_button = new IconButton(":/img/material/delete.svg", this);
    connect(save_button, &IconButton::clicked, this, &ChatWindow::onSaveChat);
    connect(clear_button, &IconButton::clicked, this, &ChatWindow::onClearChat);
    app_bar_->setActions({ save_button, clear_button });

    connect(view_, &ChatView::sig_sendText, this, &ChatWindow::onSendText);
    connect(view_, &ChatView::sig_typing, this, &ChatWindow::onTyping);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
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
void ChatWindow::onStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    switch (status)
    {
        case NetworkWorker::Status::HOST_CONNECTING:
            setStatusText(tr("Connecting to host %1...").arg(session_state_->hostAddress()));
            break;

        case NetworkWorker::Status::HOST_CONNECTED:
            view_->setStatusText(QString());
            view_->setInputEnabled(true);
            break;

        case NetworkWorker::Status::HOST_DISCONNECTED:
            setStatusText(tr("The connection to the host has been lost."));
            view_->setInputEnabled(false);
            break;

        case NetworkWorker::Status::RELAY_ERROR:
            setStatusText(data.toString());
            break;

        case NetworkWorker::Status::VERSION_MISMATCH:
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
        const proto::chat::Status& chat_status = chat.chat_status();

        // Typing is transient: shown on the status line and auto-cleared, never stored.
        if (chat_status.code() == proto::chat::Status::CODE_TYPING)
        {
            const QString user = QString::fromStdString(chat_status.source());
            view_->setStatusText(tr("%1 is typing...").arg(user));
            typing_clear_timer_->start(kTypingClearMs);
            return;
        }

        const QString text = statusToString(chat_status);
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

    sendChatMessage(chat);
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
        startNewSession();
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
            startNewSession();
            return;
        }

        setStatusText(tr("Error requesting connection via router."));
    });
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::startNewSession()
{
    if (!session_keeper_)
        session_keeper_ = SessionKeeper::create(this);

    worker_manager_.reset();
    worker_manager_ = std::make_unique<WorkerManager>();

    std::unique_ptr<NetworkWorker> network_worker = std::make_unique<NetworkWorker>();
    network_worker_ = network_worker.get();
    worker_manager_->add(std::move(network_worker));

    connect(this, &ChatWindow::sig_startConnection, network_worker_, &NetworkWorker::onStartConnection,
            Qt::QueuedConnection);
    connect(this, &ChatWindow::sig_sessionReady, network_worker_, &NetworkWorker::onSessionReady,
            Qt::QueuedConnection);
    connect(this, &ChatWindow::sig_sendMessage, network_worker_, &NetworkWorker::onSendMessage,
            Qt::QueuedConnection);
    connect(network_worker_, &NetworkWorker::sig_statusChanged, this, &ChatWindow::onNetworkStatusChanged,
            Qt::QueuedConnection);

    // The text chat runs over a single session channel.
    connect(network_worker_, &NetworkWorker::sig_channel_0, this, &ChatWindow::onChannelMessage,
            Qt::QueuedConnection);

    worker_manager_->start();

    emit sig_startConnection(session_state_);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onNetworkStatusChanged(NetworkWorker::Status status, const QVariant& data)
{
    if (status == NetworkWorker::Status::HOST_DISCONNECTED && session_keeper_)
        session_keeper_->release();

    onStatusChanged(status, data);

    // HOST_CONNECTED means the handshake passed and the (still paused) channel is ready.
    if (status == NetworkWorker::Status::HOST_CONNECTED)
        onNetworkConnected();
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::onNetworkConnected()
{
    if (session_keeper_)
        session_keeper_->acquire();

    // Now the session can receive incoming messages.
    emit sig_sessionReady();
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

    onChatMessage(chat);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::sendChatMessage(const proto::chat::Chat& chat)
{
    emit sig_sendMessage(proto::peer::CHANNEL_ID_0, serialize(chat));
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::setStatusText(const QString& text)
{
    view_->setStatusText(text);
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

//--------------------------------------------------------------------------------------------------
void ChatWindow::onTyping()
{
    // Throttle: at most one typing status per interval while the user keeps editing.
    if (typing_throttle_->isActive())
        return;

    proto::chat::Chat chat;
    proto::chat::Status* status = chat.mutable_chat_status();
    status->set_timestamp(QDateTime::currentSecsSinceEpoch());
    status->set_source(display_name_.toStdString());
    status->set_code(proto::chat::Status::CODE_TYPING);

    sendChatMessage(chat);

    typing_throttle_->start(kTypingThrottleMs);
}

//--------------------------------------------------------------------------------------------------
void ChatWindow::clearTypingStatus()
{
    view_->setStatusText(QString());
}
