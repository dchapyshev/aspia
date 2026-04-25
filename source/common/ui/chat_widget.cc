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

#include "common/ui/chat_widget.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QSaveFile>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include "base/logging.h"
#include "common/ui/chat_incoming_message.h"
#include "common/ui/msg_box.h"
#include "common/ui/chat_outgoing_message.h"
#include "common/ui/chat_status_message.h"
#include "ui_chat_widget.h"

namespace common {

namespace {

const int kMaxMessageLength = 2048;
const int kMaxStoredMessages = 30;
const QString kHistoryDirName = "chat";

//--------------------------------------------------------------------------------------------------
QString currentTime()
{
    return QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
}

//--------------------------------------------------------------------------------------------------
QString historyDirectoryPath(const QString& history_id)
{
    QString base_path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base_path.isEmpty())
    {
        LOG(ERROR) << "Unable to get app config location";
        return QString();
    }

    base_path += "/aspia";

    if (history_id.isEmpty())
        return base_path;

    QDir dir(base_path);
    if (!dir.mkpath(kHistoryDirName))
    {
        LOG(ERROR) << "Unable to create directory";
        return QString();
    }

    return dir.filePath(kHistoryDirName);
}

//--------------------------------------------------------------------------------------------------
QString historyFilePath(const QString& history_id)
{
    QString dir_path = historyDirectoryPath(history_id);
    if (dir_path.isEmpty())
        return QString();

    if (history_id.isEmpty())
        return QDir(dir_path).filePath("chat.json");

    return QDir(dir_path).filePath(history_id + ".json");
}

//--------------------------------------------------------------------------------------------------
void removeHistoryFilesOlderThan(int days)
{
    if (days <= 0)
    {
        LOG(ERROR) << "Invalid history retention days:" << days;
        return;
    }

    QString dir_path = historyDirectoryPath(QString());
    if (dir_path.isEmpty())
        return;

    QDir dir(dir_path);
    if (!dir.exists())
        return;

    QDateTime expiration = QDateTime::currentDateTimeUtc().addDays(-days);
    QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files);

    for (const QFileInfo& file_info : std::as_const(files))
    {
        if (file_info.lastModified().toUTC() >= expiration)
            continue;

        if (!dir.remove(file_info.fileName()))
            LOG(ERROR) << "Unable to remove old chat history file:" << file_info.filePath();
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ChatWidget::ChatWidget(QWidget* parent)
    : QWidget(parent),
      ui(std::make_unique<Ui::ChatWidget>()),
      display_name_(QHostInfo::localHostName()),
      status_clear_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);
    ui->edit_message->installEventFilter(this);
    ui->list_messages->horizontalScrollBar()->installEventFilter(this);
    ui->list_messages->verticalScrollBar()->installEventFilter(this);

    connect(status_clear_timer_, &QTimer::timeout, ui->label_status, &QLabel::clear);
    connect(ui->button_send, &QToolButton::clicked, this, &ChatWidget::onSendMessage);
    connect(ui->button_tools, &QToolButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] Show tool menu";

        QMenu menu;
        QAction* save_chat_action = menu.addAction(tr("Save chat..."));
        QAction* clear_history_action = menu.addAction(tr("Clear chat"));

        menu.show();

        QPoint pos = ui->button_tools->mapToGlobal(ui->button_tools->rect().topRight());
        pos.setX(pos.x() - menu.rect().width());
        pos.setY(pos.y() - menu.rect().height());

        QAction* action = menu.exec(pos);
        if (action == clear_history_action)
        {
            onClearHistory();
        }
        else if (action == save_chat_action)
        {
            onSaveChat();
        }
    });

    ui->edit_message->setFocus();
}

//--------------------------------------------------------------------------------------------------
ChatWidget::~ChatWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::readMessage(const proto::chat::Message& message)
{
    QString source = QString::fromStdString(message.source());
    QString text = QString::fromStdString(message.text());

    addIncomingMessage(message.timestamp(), source, text);
    appendHistoryMessage(message.timestamp(), source, text, false);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::readStatus(const proto::chat::Status& status)
{
    status_clear_timer_->stop();

    QString user_name = QString::fromStdString(status.source());
    QString message;

    switch (status.code())
    {
        case proto::chat::Status::CODE_TYPING:
            ui->label_status->setText(tr("%1 is typing...").arg(user_name));
            break;

        case proto::chat::Status::CODE_STARTED:
            message = tr("User %1 has joined the chat (%2)").arg(user_name, currentTime());
            break;

        case proto::chat::Status::CODE_STOPPED:
            message = tr("User %1 has left the chat (%2)").arg(user_name, currentTime());
            break;

        case proto::chat::Status::CODE_USER_CONNECTED:
            message = tr("User %1 is logged in (%2)").arg(user_name, currentTime());
            break;

        case proto::chat::Status::CODE_USER_DISCONNECTED:
            message = tr("User %1 is not logged in (%2)").arg(user_name, currentTime());
            break;

        case proto::chat::Status::CODE_NO_USERS:
            message = tr("There are no connected users (%1)").arg(currentTime());
            break;

        default:
            LOG(ERROR) << "Unhandled status code:" << status.code();
            return;
    }

    if (!message.isEmpty())
    {
        addStatusMessage(message);
        appendHistoryStatus(message);
    }

    status_clear_timer_->start(std::chrono::seconds(1));
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::setDisplayName(const QString& display_name)
{
    display_name_ = display_name;

    if (display_name_.isEmpty())
        display_name_ = QHostInfo::localHostName();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::setHistoryId(const QString& history_id)
{
    history_id_ = history_id;
    loadHistory();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::setChatEnabled(bool enable)
{
    ui->edit_message->setEnabled(enable);
    ui->button_send->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
bool ChatWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui->edit_message && event->type() == QEvent::KeyPress)
    {
        QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Return)
        {
            onSendMessage();
            return true;
        }

        onSendStatus(proto::chat::Status::CODE_TYPING);
    }
    else if (object == ui->list_messages->horizontalScrollBar() ||
             object == ui->list_messages->verticalScrollBar())
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide)
        {
            onUpdateSize();
        }
    }

    return QWidget::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &ChatWidget::onUpdateSize);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::resizeEvent(QResizeEvent* /* event */)
{
    onUpdateSize();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event detected";

    emit sig_textChatClosed();
    QWidget::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::addIncomingMessage(time_t timestamp, const QString& source, const QString& message)
{
    QListWidget* list_messages = ui->list_messages;
    ChatIncomingMessage* message_widget = new ChatIncomingMessage(list_messages);

    message_widget->setTimestamp(timestamp);
    message_widget->setSource(source);
    message_widget->setMessageText(message);

    QListWidgetItem* item = new QListWidgetItem(list_messages);
    list_messages->setItemWidget(item, message_widget);

    onUpdateSize();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::addOutgoingMessage(time_t timestamp, const QString& message)
{
    QListWidget* list_messages = ui->list_messages;
    ChatOutgoingMessage* message_widget = new ChatOutgoingMessage(list_messages);

    message_widget->setTimestamp(timestamp);
    message_widget->setMessageText(message);

    QListWidgetItem* item = new QListWidgetItem(list_messages);
    list_messages->setItemWidget(item, message_widget);

    onUpdateSize();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::addStatusMessage(const QString& message)
{
    QListWidget* list_messages = ui->list_messages;
    ChatStatusMessage* message_widget = new ChatStatusMessage(list_messages);

    message_widget->setMessageText(message);

    QListWidgetItem* item = new QListWidgetItem(list_messages);
    list_messages->setItemWidget(item, message_widget);

    onUpdateSize();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::appendHistoryMessage(
    qint64 timestamp, const QString& source, const QString& text, bool outgoing)
{
    history_messages_.push_back(HistoryMessage{ timestamp, source, text, outgoing, false });

    while (history_messages_.size() > kMaxStoredMessages)
        history_messages_.pop_front();

    saveHistory();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::appendHistoryStatus(const QString& text)
{
    history_messages_.push_back(HistoryMessage{ 0, QString(), text, false, true });

    while (history_messages_.size() > kMaxStoredMessages)
        history_messages_.pop_front();

    saveHistory();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::loadHistory()
{
    clearMessages();
    history_messages_.clear();

    removeHistoryFilesOlderThan(30);

    QString file_path = historyFilePath(history_id_);
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Unable to get history file path";
        return;
    }

    LOG(INFO) << "Load chat history from" << file_path;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open history file";
        return;
    }

    QByteArray buffer = file.readAll();
    if (buffer.isEmpty())
    {
        LOG(INFO) << "Empty history file";
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(buffer);
    if (!document.isArray())
    {
        LOG(ERROR) << "Invalid chat history format";
        return;
    }

    QVector<HistoryMessage> loaded_messages;

    const QJsonArray items = document.array();
    loaded_messages.reserve(items.size());

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

        loaded_messages.push_back(message);
    }

    while (loaded_messages.size() > kMaxStoredMessages)
        loaded_messages.pop_front();

    history_messages_ = loaded_messages;

    for (const HistoryMessage& message : std::as_const(history_messages_))
    {
        if (message.status)
            addStatusMessage(message.text);
        else if (message.outgoing)
            addOutgoingMessage(message.timestamp, message.text);
        else
            addIncomingMessage(message.timestamp, message.source, message.text);
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::saveHistory() const
{
    QString file_path = historyFilePath(history_id_);
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Unable to get history file path";
        return;
    }

    if (history_messages_.isEmpty())
    {
        LOG(INFO) << "Remove history file";
        QFile::remove(file_path);
        return;
    }

    QJsonArray items;
    for (const HistoryMessage& message : std::as_const(history_messages_))
    {
        QJsonObject item;
        item["timestamp"] = static_cast<qint64>(message.timestamp);
        item["source"] = message.source;
        item["text"] = message.text;
        item["outgoing"] = message.outgoing;
        item["status"] = message.status;
        items.push_back(item);
    }

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open chat history file for writing:" << file_path;
        return;
    }

    file.write(QJsonDocument(items).toJson(QJsonDocument::Compact));
    if (!file.commit())
    {
        LOG(ERROR) << "Unable to write chat history";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::clearMessages()
{
    QListWidget* list_messages = ui->list_messages;
    for (int i = list_messages->count() - 1; i >= 0; --i)
        delete list_messages->item(i);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::onSendMessage()
{
    QLineEdit* edit_message = ui->edit_message;
    QString message = edit_message->text();
    if (message.isEmpty())
        return;

    if (message.length() > kMaxMessageLength)
    {
        LOG(ERROR) << "Too long message:" << message.length();
        common::MsgBox::warning(this,
                             tr("The message is too long. The maximum message length is %n "
                                "characters.", "", kMaxMessageLength));
        return;
    }

    qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    addOutgoingMessage(timestamp, message);
    appendHistoryMessage(timestamp, display_name_, message, true);
    edit_message->clear();
    edit_message->setFocus();

    proto::chat::Message chat_message;
    chat_message.set_timestamp(timestamp);
    chat_message.set_source(display_name_.toStdString());
    chat_message.set_text(message.toStdString());

    emit sig_sendMessage(chat_message);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::onSendStatus(proto::chat::Status::Code code)
{
    proto::chat::Status chat_status;
    chat_status.set_timestamp(QDateTime::currentSecsSinceEpoch());
    chat_status.set_source(display_name_.toStdString());
    chat_status.set_code(code);

    emit sig_sendStatus(chat_status);
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::onClearHistory()
{
    LOG(INFO) << "[ACTION] Clear history";

    clearMessages();
    history_messages_.clear();
    saveHistory();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::onSaveChat()
{
    LOG(INFO) << "[ACTION] Save chat";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("TXT files (*.txt)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "File path not selected";
        return;
    }

    LOG(INFO) << "Selected file path:" << file_path;

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(ERROR) << "Unable to open file:" << file.errorString();
        common::MsgBox::warning(this,
                             tr("Could not open file for writing."));
        return;
    }

    QListWidget* list_messages = ui->list_messages;
    QTextStream stream(&file);

    for (int i = 0; i < list_messages->count(); ++i)
    {
        QListWidgetItem* item = list_messages->item(i);
        ChatMessage* message_widget =
            static_cast<ChatMessage*>(list_messages->itemWidget(item));

        if (message_widget->direction() == ChatMessage::Direction::INCOMING)
        {
            ChatIncomingMessage* incoming_message_widget =
                static_cast<ChatIncomingMessage*>(message_widget);

            stream << "[" << incoming_message_widget->messageTime() << "] "
                   << incoming_message_widget->source() << Qt::endl;
            stream << incoming_message_widget->messageText() << Qt::endl;
            stream << Qt::endl;
        }
        else if (message_widget->direction() == ChatMessage::Direction::OUTGOING)
        {
            ChatOutgoingMessage* outgoing_message_widget =
                static_cast<ChatOutgoingMessage*>(message_widget);

            stream << "[" << outgoing_message_widget->messageTime() << "] " << Qt::endl;
            stream << outgoing_message_widget->messageText() << Qt::endl;
            stream << Qt::endl;
        }
        else
        {
            DCHECK_EQ(message_widget->direction(), ChatMessage::Direction::STATUS);

            ChatStatusMessage* status_message_widget =
                static_cast<ChatStatusMessage*>(message_widget);

            stream << status_message_widget->messageText() << Qt::endl;
            stream << Qt::endl;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::onUpdateSize()
{
    QListWidget* list_messages = ui->list_messages;
    int viewport_width = list_messages->viewport()->width();
    if (viewport_width <= 0)
        return;

    int count = list_messages->count();

    for (int i = 0; i < count; ++i)
    {
        QListWidgetItem* item = list_messages->item(i);

        ChatMessage* message_widget =
            static_cast<ChatMessage*>(list_messages->itemWidget(item));
        if (!message_widget)
            continue;

        message_widget->setFixedWidth(viewport_width);
        message_widget->setFixedHeight(message_widget->heightForWidth(viewport_width));

        item->setSizeHint(message_widget->size());
    }

    list_messages->scrollToBottom();
}

} // namespace common
