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
#include <QFile>
#include <QFileDialog>
#include <QHostInfo>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QTimer>

#include "base/logging.h"
#include "common/ui/chat_incoming_message.h"
#include "common/ui/chat_outgoing_message.h"
#include "common/ui/chat_status_message.h"
#include "ui_chat_widget.h"

namespace common {

namespace {

const int kMaxMessageLength = 2048;

QString currentTime()
{
    return QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat);
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
    QListWidget* list_messages = ui->list_messages;
    ChatIncomingMessage* message_widget = new ChatIncomingMessage(list_messages);

    message_widget->setTimestamp(message.timestamp());
    message_widget->setSource(QString::fromStdString(message.source()));
    message_widget->setMessageText(QString::fromStdString(message.text()));

    QListWidgetItem* item = new QListWidgetItem(list_messages);
    list_messages->setItemWidget(item, message_widget);

    onUpdateSize();
}

//--------------------------------------------------------------------------------------------------
void ChatWidget::readStatus(const proto::chat::Status& status)
{
    status_clear_timer_->stop();

    QString user_name = QString::fromStdString(status.source());

    switch (status.code())
    {
        case proto::chat::Status::CODE_TYPING:
            ui->label_status->setText(tr("%1 is typing...").arg(user_name));
            break;

        case proto::chat::Status::CODE_STARTED:
            addStatusMessage(tr("User %1 has joined the chat (%2)").arg(user_name, currentTime()));
            break;

        case proto::chat::Status::CODE_STOPPED:
            addStatusMessage(tr("User %1 has left the chat (%2)").arg(user_name, currentTime()));
            break;

        case proto::chat::Status::CODE_USER_CONNECTED:
            addStatusMessage(tr("User %1 is logged in (%2)").arg(user_name, currentTime()));
            break;

        case proto::chat::Status::CODE_USER_DISCONNECTED:
            addStatusMessage(tr("User %1 is not logged in (%2)").arg(user_name, currentTime()));
            break;

        default:
            LOG(ERROR) << "Unhandled status code:" << status.code();
            return;
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
void ChatWidget::onSendMessage()
{
    QLineEdit* edit_message = ui->edit_message;
    QString message = edit_message->text();
    if (message.isEmpty())
        return;

    if (message.length() > kMaxMessageLength)
    {
        LOG(ERROR) << "Too long message:" << message.length();
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The message is too long. The maximum message length is %n "
                                "characters.", "", kMaxMessageLength),
                             QMessageBox::Ok);
        return;
    }

    qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    addOutgoingMessage(timestamp, message);
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

    QListWidget* list_messages = ui->list_messages;
    for (int i = list_messages->count() - 1; i >= 0; --i)
        delete list_messages->item(i);
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
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Could not open file for writing."),
                             QMessageBox::Ok);
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
    int count = list_messages->count();

    for (int i = 0; i < count; ++i)
    {
        QListWidgetItem* item = list_messages->item(i);

        ChatMessage* message_widget =
            static_cast<ChatMessage*>(list_messages->itemWidget(item));
        if (!message_widget)
            continue;

        int viewport_width = list_messages->viewport()->width();

        message_widget->setFixedWidth(viewport_width);
        message_widget->setFixedHeight(message_widget->heightForWidth(viewport_width));

        item->setSizeHint(message_widget->size());
    }

    list_messages->scrollToBottom();
}

} // namespace common
