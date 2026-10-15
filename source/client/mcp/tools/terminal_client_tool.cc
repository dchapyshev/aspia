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

#include "client/mcp/tools/terminal_client_tool.h"

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyCombination>
#include <QPoint>
#include <QRegularExpression>
#include <QSize>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <optional>

#include "base/time_types.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

namespace {

const int kMaxPictureSize = 1280;
const int kMaxWait = 60;
const Seconds kSettleTime{ 1 };
const MilliSeconds kWaitInterval{ 100 };

const struct
{
    const char* name;
    Qt::KeyboardModifier modifier;
} kModifiers[] =
{
    { "ctrl", Qt::ControlModifier },
    { "shift", Qt::ShiftModifier },
    { "alt", Qt::AltModifier }
};

const struct
{
    const char* name;
    int qt_key;
} kKeys[] =
{
    { "escape", Qt::Key_Escape },
    { "tab", Qt::Key_Tab },
    { "backspace", Qt::Key_Backspace },
    { "enter", Qt::Key_Return },
    { "insert", Qt::Key_Insert },
    { "delete", Qt::Key_Delete },
    { "home", Qt::Key_Home },
    { "end", Qt::Key_End },
    { "left", Qt::Key_Left },
    { "up", Qt::Key_Up },
    { "right", Qt::Key_Right },
    { "down", Qt::Key_Down },
    { "pageup", Qt::Key_PageUp },
    { "pagedown", Qt::Key_PageDown },
    { "space", Qt::Key_Space }
};

//--------------------------------------------------------------------------------------------------
std::optional<int> keyArgument(const QString& name)
{
    if (name.size() == 1)
        return name.at(0).unicode();

    const int function_number = name.startsWith('f') ? name.mid(1).toInt() : 0;
    if (function_number >= 1 && function_number <= 24)
        return Qt::Key_F1 + function_number - 1;

    for (const auto& key : kKeys)
    {
        if (name == QLatin1String(key.name))
            return key.qt_key;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
std::optional<QKeyCombination> keysArgument(const QJsonValue& value)
{
    const QStringList names = value.toString().toLower().split('+');
    Qt::KeyboardModifiers modifiers;

    for (qsizetype i = 0; i < names.size() - 1; ++i)
    {
        const QString name = names[i].trimmed();
        auto modifier = std::find_if(std::begin(kModifiers), std::end(kModifiers),
                                     [&name](const auto& entry) { return name == QLatin1String(entry.name); });
        if (modifier == std::end(kModifiers))
            return std::nullopt;

        modifiers |= modifier->modifier;
    }

    std::optional<int> key = keyArgument(names.back().trimmed());
    if (!key.has_value())
        return std::nullopt;

    return QKeyCombination(modifiers, static_cast<Qt::Key>(*key));
}

//--------------------------------------------------------------------------------------------------
bool untilMatched(const McpController::TerminalWindow& window, const QRegularExpression& until)
{
    // Until the host echoes the last input, the line of the cursor still holds the previous prompt.
    if (window.lastOutputTime() <= window.lastInputTime())
        return false;

    const QStringList lines = window.lines(0);
    const int row = window.cursorPosition().y();

    return row >= 0 && row < lines.size() && until.match(lines[row]).hasMatch();
}

//--------------------------------------------------------------------------------------------------
QString screenText(
    const McpController::TerminalWindow& window, int history, const std::optional<QRegularExpression>& until)
{
    QStringList lines = window.lines(history);
    const QSize grid_size = window.gridSize();
    const QPoint cursor = window.cursorPosition();
    const int cursor_line = static_cast<int>(lines.size()) - grid_size.height() + cursor.y();

    while (lines.size() > cursor_line + 1 && lines.back().isEmpty())
        lines.removeLast();

    QJsonObject content{ { "columns", grid_size.width() },
                         { "rows", grid_size.height() },
                         { "login_prompt", window.hasLoginPrompt() },
                         { "cursor_line", cursor_line },
                         { "cursor_column", cursor.x() },
                         { "lines", QJsonArray::fromStringList(lines) } };

    if (until.has_value())
        content.insert("matched", untilMatched(window, *until));

    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString TerminalClientTool::name() const
{
    return "terminal_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject TerminalClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a terminal client window from list_clients." } };

    QJsonObject action{ { "type", "string" },
                        { "enum", QJsonArray{ "sign_in", "read", "screenshot", "type", "key" } },
                        { "description", "sign_in signs in to the host with the saved credentials that are chosen "
                                         "for it in Aspia Client, the credentials are not shown to you. read returns "
                                         "the text of the terminal. screenshot returns the picture of the terminal "
                                         "that the user sees in the window, with the colors that full-screen "
                                         "programs like Far or mc use to show the selection. type enters text, key "
                                         "presses a key." } };

    QJsonObject history{ { "type", "integer" },
                         { "minimum", 0 },
                         { "description", "Number of the lines above the screen that read returns too. 0 by "
                                          "default." } };

    QJsonObject wait{ { "type", "integer" },
                      { "minimum", 0 },
                      { "maximum", kMaxWait },
                      { "description", "Seconds that read waits for the output of the host. Without until read "
                                       "returns once the host sends nothing for a second or the time is over. 0 by "
                                       "default." } };

    QJsonObject until{ { "type", "string" },
                       { "description", "Regular expression that read waits for on the line of the cursor, usually "
                                        "the prompt of the shell that is seen in an earlier read, for example "
                                        "^PS .*>$ or \\$$. The trailing spaces of a line are not part of it. With "
                                        "until read does not return at a second of silence, and matched in the "
                                        "result says whether the line matched after the output for the last type "
                                        "or key, so a command that still runs gives false." } };

    QJsonObject text{ { "type", "string" },
                      { "description", "Text of type. A new line is sent as Enter, so end a command with a new line "
                                       "to run it." } };

    QJsonObject keys{ { "type", "string" },
                      { "description", "Key of key with its modifiers joined by +, for example enter, ctrl+c, ctrl+d, "
                                       "shift+tab, alt+x, escape, up or f10. The modifiers are ctrl, shift and alt. "
                                       "The keys are a character, f1 to f24, escape, tab, backspace, enter, insert, "
                                       "delete, home, end, left, up, right, down, pageup, pagedown and space." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that is connected to the terminal of a host. "
                         "The window must be opened before, for example by connect_local_host or "
                         "connect_router_host with the terminal session_type, and its client_id comes from "
                         "list_clients. The window asks for the user name and password of the host first, and "
                         "type and key work only after sign_in or after the user signed in. read returns the "
                         "lines of the terminal with the position of the cursor, login_prompt is true while the "
                         "window asks for credentials. sign_in, type and key return no text, so read or take a "
                         "screenshot to see the result. The user sees every action in the window." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "history", history },
                                                                   { "wait", wait },
                                                                   { "until", until },
                                                                   { "text", text },
                                                                   { "keys", keys } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void TerminalClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "sign_in" && action != "read" && action != "screenshot" && action != "type" && action != "key")
    {
        reply(false, "The action must be one of sign_in, read, screenshot, type, key.");
        return;
    }

    McpController::TerminalWindow* window = McpController::instance().terminalWindow(*client_id);
    if (!window)
    {
        if (hasClient(*client_id))
            reply(false, QString("The client %1 is not a terminal client.").arg(*client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    if (action == "sign_in")
    {
        switch (window->signIn())
        {
            case McpController::SignInResult::STARTED:
                reply(true, QString("The client %1 signs in to the host with the saved credentials. Read the "
                                    "terminal to see the result.").arg(*client_id));
                break;
            case McpController::SignInResult::NOT_REQUESTED:
                reply(false, QString("The client %1 does not ask for credentials now.").arg(*client_id));
                break;
            case McpController::SignInResult::NOT_SELECTED:
                reply(false, QString("No saved credentials are chosen for the host of the client %1, the user "
                                     "signs in.").arg(*client_id));
                break;
        }
        return;
    }

    if (action == "read")
    {
        std::optional<qint64> history = 0;
        if (arguments.contains("history"))
            history = integerArgument(arguments.value("history"));

        if (!history.has_value() || *history < 0)
        {
            reply(false, "The history must be a non-negative integer.");
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

        std::optional<QRegularExpression> until;
        if (arguments.contains("until"))
        {
            until.emplace(arguments.value("until").toString());
            if (until->pattern().isEmpty())
            {
                reply(false, "The until must be a non-empty regular expression.");
                return;
            }

            if (!until->isValid())
            {
                reply(false, QString("The until is not a valid regular expression. %1").arg(until->errorString()));
                return;
            }
        }

        const int history_lines = static_cast<int>(std::min<qint64>(*history, std::numeric_limits<int>::max()));

        if (*wait == 0)
        {
            reply(true, screenText(*window, history_lines, until));
            return;
        }

        const TimePoint start = Clock::now();
        const TimePoint deadline = start + Seconds(*wait);

        QTimer* timer = new QTimer(&McpController::instance());
        QObject::connect(timer, &QTimer::timeout, timer,
                         [timer, client_id = *client_id, history_lines, until, start, deadline, reply]()
        {
            McpController::TerminalWindow* window = McpController::instance().terminalWindow(client_id);
            const TimePoint now = Clock::now();

            if (window && now < deadline)
            {
                const bool done = until.has_value() ? untilMatched(*window, *until) :
                                                      now - std::max(start, window->lastOutputTime()) >= kSettleTime;
                if (!done)
                    return;
            }

            timer->stop();
            timer->deleteLater();

            if (!window)
            {
                reply(false, QString("The client %1 was closed while read waited.").arg(client_id));
                return;
            }

            reply(true, screenText(*window, history_lines, until));
        });
        timer->start(kWaitInterval);
        return;
    }

    if (action == "screenshot")
    {
        QImage image = window->screenshot();
        if (image.isNull())
        {
            reply(false, QString("Unable to take the picture of the client %1.").arg(*client_id));
            return;
        }

        if (image.width() > kMaxPictureSize || image.height() > kMaxPictureSize)
            image = image.scaled(kMaxPictureSize, kMaxPictureSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        {
            reply(false, "Unable to encode the picture.");
            return;
        }

        reply(true, QString("The picture of the terminal of the client %1 is %2x%3 pixels.")
            .arg(*client_id).arg(image.width()).arg(image.height()), png);
        return;
    }

    if (window->hasLoginPrompt())
    {
        reply(false, QString("The client %1 asks for credentials. Use sign_in, or the user signs in.")
            .arg(*client_id));
        return;
    }

    if (action == "type")
    {
        const QString text = arguments.value("text").toString();
        if (text.isEmpty())
        {
            reply(false, "The text of type must be a non-empty string.");
            return;
        }

        window->sendText(text);
    }
    else
    {
        std::optional<QKeyCombination> keys = keysArgument(arguments.value("keys"));
        if (!keys.has_value())
        {
            reply(false, "The keys must be one key with its modifiers, for example ctrl+c. The description of keys "
                         "lists the names.");
            return;
        }

        window->sendKey(keys->key(), keys->keyboardModifiers());
    }

    reply(true, QString("The client %1 got %2. Read the terminal to see the result.").arg(*client_id).arg(action));
}
