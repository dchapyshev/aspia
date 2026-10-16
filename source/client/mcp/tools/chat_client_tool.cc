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

#include "client/mcp/tools/chat_client_tool.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <optional>

#include "base/time_types.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

namespace {

const int kMaxWait = 60;
const MilliSeconds kWaitInterval{ 100 };

//--------------------------------------------------------------------------------------------------
QString directionName(ChatMessage::Direction direction)
{
    switch (direction)
    {
        case ChatMessage::Direction::INCOMING:
            return "incoming";
        case ChatMessage::Direction::OUTGOING:
            return "outgoing";
        case ChatMessage::Direction::STATUS:
            return "status";
    }

    return "status";
}

//--------------------------------------------------------------------------------------------------
bool hasAnswer(const QList<ChatWidget::Message>& messages, qint64 after)
{
    for (qint64 i = after; i < messages.size(); ++i)
    {
        if (messages[i].direction != ChatMessage::Direction::OUTGOING)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
QString messagesText(const McpController::ChatWindow& window, qint64 after)
{
    const QList<ChatWidget::Message> messages = window.messages();

    // The user cleared the chat, so the ids start again.
    if (after > messages.size())
        after = 0;

    QJsonArray list;
    for (qint64 i = after; i < messages.size(); ++i)
    {
        const ChatWidget::Message& message = messages[i];

        QJsonObject entry{ { "id", i }, { "direction", directionName(message.direction) },
                           { "text", message.text } };
        if (!message.time.isEmpty())
            entry.insert("time", message.time);
        if (!message.source.isEmpty())
            entry.insert("source", message.source);

        list.append(entry);
    }

    QJsonObject content{ { "count", messages.size() }, { "messages", list } };

    const QString status = window.chatStatus();
    if (!status.isEmpty())
        content.insert("status", status);

    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
void waitAnswer(qint64 client_id, qint64 after, qint64 wait, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + Seconds(wait);

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, after, deadline, reply]()
    {
        McpController::ChatWindow* window = McpController::instance().chatWindow(client_id);

        if (window && !hasAnswer(window->messages(), after) && Clock::now() < deadline)
            return;

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        reply(true, messagesText(*window, after));
    });
    timer->start(kWaitInterval);
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString ChatClientTool::name() const
{
    return "chat_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ChatClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a chat client window from list_clients." } };

    QJsonObject action{ { "type", "string" },
                        { "enum", QJsonArray{ "read", "send" } },
                        { "description", "read returns the messages that the window shows. send enters text in "
                                         "the message field of the window and sends it as the user does." } };

    QJsonObject text{ { "type", "string" },
                      { "description", QString("Text of send, at most %1 characters.")
                          .arg(ChatWidget::kMaxMessageLength) } };

    QJsonObject after{ { "type", "integer" },
                       { "minimum", 0 },
                       { "description", "Id from which read returns the messages, usually the count of an "
                                        "earlier read or send. 0 by default." } };

    QJsonObject wait{ { "type", "integer" },
                      { "minimum", 0 },
                      { "maximum", kMaxWait },
                      { "description", "Seconds that read waits for an incoming or status message with the id "
                                       "after or later. 0 by default." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that has the text chat with a host. The window "
                         "must be opened before, for example by connect_local_host or connect_router_host with the "
                         "chat session_type, and its client_id comes from list_clients. The messages have ids in "
                         "the order of the window. incoming messages come from the people at the host, outgoing "
                         "ones are sent from this window, and status messages say that someone joined or left the "
                         "chat, in the language of Aspia Client. The status of the result says that the other "
                         "side is typing. The user sees every message that is sent." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "text", text },
                                                                   { "after", after },
                                                                   { "wait", wait } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ChatClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    const QString session_type_error = sessionTypeError(proto::peer::SESSION_TYPE_CHAT);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "read" && action != "send")
    {
        reply(false, "The action must be one of read, send.");
        return;
    }

    McpController::ChatWindow* window = McpController::instance().chatWindow(*client_id);
    if (!window)
    {
        if (hasClient(*client_id))
            reply(false, QString("The client %1 is not a chat client.").arg(*client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    if (action == "read")
    {
        std::optional<qint64> after = 0;
        if (arguments.contains("after"))
            after = integerArgument(arguments.value("after"));

        if (!after.has_value() || *after < 0)
        {
            reply(false, "The after must be an integer from 0.");
            return;
        }

        std::optional<qint64> wait = 0;
        if (arguments.contains("wait"))
            wait = integerArgument(arguments.value("wait"));

        if (!wait.has_value() || *wait < 0 || *wait > kMaxWait)
        {
            reply(false, QString("The wait must be an integer from 0 to %1.").arg(kMaxWait));
            return;
        }

        if (*wait == 0)
            reply(true, messagesText(*window, *after));
        else
            waitAnswer(*client_id, *after, *wait, reply);
        return;
    }

    const QJsonValue text = arguments.value("text");
    if (!text.isString() || text.toString().isEmpty() ||
        text.toString().length() > ChatWidget::kMaxMessageLength)
    {
        reply(false, QString("The text must be a string of 1 to %1 characters.").arg(ChatWidget::kMaxMessageLength));
        return;
    }

    if (!isClientConnected(*client_id))
    {
        reply(false, QString("The client %1 is not connected to the host. Its state is in list_clients.")
            .arg(*client_id));
        return;
    }

    if (!window->sendText(text.toString()))
    {
        reply(false, QString("The client %1 can not send now. Its message field is turned off or holds the text "
                             "that the user types.").arg(*client_id));
        return;
    }

    reply(true, QString("The message is sent. Use read with after %1 and wait to get the answer.")
        .arg(window->messages().size()));
}
