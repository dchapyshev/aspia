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

#include "client/mcp/tools/desktop_client_tool.h"

#include <QBuffer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <optional>

#include "base/time_types.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"
#include "proto/desktop_screen.h"

namespace {

const int kMaxPictureSize = 1280;
const int kDragSteps = 10;
const MilliSeconds kDoubleClickDelay{ 150 };
const int kDefaultScrollAmount = 3;
const int kMaxScrollAmount = 20;

const struct
{
    const char* name;
    quint32 usb_keycode;
} kModifiers[] =
{
    { "ctrl", 0x0700e0 },
    { "shift", 0x0700e1 },
    { "alt", 0x0700e2 },
    { "win", 0x0700e3 },
    { "meta", 0x0700e3 },
    { "super", 0x0700e3 },
    { "cmd", 0x0700e3 }
};

const struct
{
    const char* name;
    int qt_keycode;
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
    { "space", Qt::Key_Space },
    { "printscreen", Qt::Key_Print },
    { "pause", Qt::Key_Pause }
};

//--------------------------------------------------------------------------------------------------
QSize pictureSize(const QSize& frame_size)
{
    if (frame_size.width() <= kMaxPictureSize && frame_size.height() <= kMaxPictureSize)
        return frame_size;

    return frame_size.scaled(kMaxPictureSize, kMaxPictureSize, Qt::KeepAspectRatio);
}

//--------------------------------------------------------------------------------------------------
std::optional<QPoint> pointArgument(const QJsonObject& arguments, const QString& x_name, const QString& y_name,
                                    const QSize& picture_size)
{
    std::optional<qint64> x = integerArgument(arguments.value(x_name));
    std::optional<qint64> y = integerArgument(arguments.value(y_name));

    if (!x.has_value() || !y.has_value() || *x < 0 || *y < 0 ||
        *x >= picture_size.width() || *y >= picture_size.height())
    {
        return std::nullopt;
    }

    return QPoint(static_cast<int>(*x), static_cast<int>(*y));
}

//--------------------------------------------------------------------------------------------------
std::optional<quint32> buttonArgument(const QJsonValue& value)
{
    if (value.isUndefined() || value.toString() == "left")
        return proto::input::MouseEvent::LEFT_BUTTON;
    if (value.toString() == "right")
        return proto::input::MouseEvent::RIGHT_BUTTON;
    if (value.toString() == "middle")
        return proto::input::MouseEvent::MIDDLE_BUTTON;

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
std::optional<quint32> wheelArgument(const QJsonValue& value)
{
    if (value.toString() == "up")
        return proto::input::MouseEvent::WHEEL_UP;
    if (value.toString() == "down")
        return proto::input::MouseEvent::WHEEL_DOWN;
    if (value.toString() == "left")
        return proto::input::MouseEvent::WHEEL_LEFT;
    if (value.toString() == "right")
        return proto::input::MouseEvent::WHEEL_RIGHT;

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
void sendMouse(McpController::DesktopWindow* window, const QPoint& point, const QSize& picture_size,
               const QSize& frame_size, quint32 mask)
{
    proto::input::MouseEvent event;
    event.set_x(point.x() * frame_size.width() / picture_size.width());
    event.set_y(point.y() * frame_size.height() / picture_size.height());
    event.set_mask(mask);
    window->mcpSendMouseEvent(event);
}

//--------------------------------------------------------------------------------------------------
quint32 keyUsbKeycode(const QString& name)
{
    for (const auto& modifier : kModifiers)
    {
        if (name == QLatin1String(modifier.name))
            return modifier.usb_keycode;
    }

    if (name.size() == 1)
        return KeycodeConverter::qtKeycodeToUsbKeycode(name.at(0).toUpper().unicode());

    const int function_number = name.startsWith('f') ? name.mid(1).toInt() : 0;
    if (function_number >= 1 && function_number <= 24)
        return KeycodeConverter::qtKeycodeToUsbKeycode(Qt::Key_F1 + function_number - 1);

    for (const auto& key : kKeys)
    {
        if (name == QLatin1String(key.name))
            return KeycodeConverter::qtKeycodeToUsbKeycode(key.qt_keycode);
    }

    return KeycodeConverter::invalidUsbKeycode();
}

//--------------------------------------------------------------------------------------------------
bool isModifier(quint32 usb_keycode)
{
    for (const auto& modifier : kModifiers)
    {
        if (usb_keycode == modifier.usb_keycode)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
std::optional<QList<quint32>> keysArgument(const QJsonValue& value)
{
    const QStringList names = value.toString().toLower().split('+');
    QList<quint32> usb_keycodes;

    for (qsizetype i = 0; i < names.size(); ++i)
    {
        const quint32 usb_keycode = keyUsbKeycode(names[i].trimmed());
        if (usb_keycode == KeycodeConverter::invalidUsbKeycode())
            return std::nullopt;

        if (i != names.size() - 1 && !isModifier(usb_keycode))
            return std::nullopt;

        usb_keycodes.append(usb_keycode);
    }

    return usb_keycodes;
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString DesktopClientTool::name() const
{
    return "desktop_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject DesktopClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a desktop client window from list_clients." } };

    QJsonObject action{ { "type", "string" },
                        { "enum", QJsonArray{ "screenshot", "move", "click", "double_click", "drag", "scroll",
                                              "key", "type", "list_screens", "select_screen", "sign_in" } },
                        { "description", "screenshot returns the picture of the remote desktop that the user "
                                         "sees in the window. move, click, double_click and scroll act at x, y. "
                                         "drag presses the button at x, y, moves to to_x, to_y and releases the "
                                         "button there. key presses keys, type enters text. list_screens returns "
                                         "the monitors of the host, select_screen shows the monitor screen_id in "
                                         "the window. sign_in signs in to the login or lock screen of the host "
                                         "with the saved credentials that are chosen for it in Aspia Client, the "
                                         "credentials are not shown to you." } };

    QJsonObject x{ { "type", "integer" },
                   { "minimum", 0 },
                   { "description", "Horizontal position in the pixels of the picture from screenshot." } };

    QJsonObject y{ { "type", "integer" },
                   { "minimum", 0 },
                   { "description", "Vertical position in the pixels of the picture from screenshot." } };

    QJsonObject to_x{ { "type", "integer" },
                      { "minimum", 0 },
                      { "description", "Horizontal end position of drag." } };

    QJsonObject to_y{ { "type", "integer" },
                      { "minimum", 0 },
                      { "description", "Vertical end position of drag." } };

    QJsonObject button{ { "type", "string" },
                        { "enum", QJsonArray{ "left", "right", "middle" } },
                        { "description", "Button of click, double_click and drag. left by default." } };

    QJsonObject direction{ { "type", "string" },
                           { "enum", QJsonArray{ "up", "down", "left", "right" } },
                           { "description", "Direction of scroll." } };

    QJsonObject amount{ { "type", "integer" },
                        { "minimum", 1 },
                        { "maximum", kMaxScrollAmount },
                        { "description", "Number of wheel steps of scroll. 3 by default." } };

    QJsonObject keys{ { "type", "string" },
                      { "description", "Key of key with its modifiers joined by +, for example ctrl+c, alt+f4, "
                                       "ctrl+shift+escape, win+e or enter. The modifiers are ctrl, shift, alt and "
                                       "win (also meta, super or cmd). The keys are a character of the US layout, "
                                       "f1 to f24, escape, tab, backspace, enter, insert, delete, home, end, left, "
                                       "up, right, down, pageup, pagedown, space, printscreen and pause. A modifier "
                                       "alone, for example win, is pressed as a key." } };

    QJsonObject screen_id{ { "type", "integer" },
                           { "description", "Id of a monitor from list_screens for select_screen." } };

    QJsonObject text{ { "type", "string" },
                      { "description", "Text of type. It is entered as characters, so the keyboard layout of the "
                                       "host does not matter. New lines and tabs are not entered on every host, "
                                       "press them by key." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that is connected to the desktop of a host. "
                         "The window must be opened before, for example by connect_local_host or "
                         "connect_router_host with the desktop session_type, and its client_id comes from "
                         "list_clients. A picture larger than 1280 pixels on a side is scaled down. The mouse "
                         "actions take the position in the pixels of that picture. The actions return no picture, "
                         "so take a screenshot to see the result. select_screen changes the picture and its size, "
                         "so the positions of an older screenshot do not fit any more. The user sees every action "
                         "in the window." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "x", x },
                                                                   { "y", y },
                                                                   { "to_x", to_x },
                                                                   { "to_y", to_y },
                                                                   { "button", button },
                                                                   { "direction", direction },
                                                                   { "amount", amount },
                                                                   { "keys", keys },
                                                                   { "text", text },
                                                                   { "screen_id", screen_id } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void DesktopClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    const QString session_type_error = sessionTypeError(proto::peer::SESSION_TYPE_DESKTOP);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "screenshot" && action != "move" && action != "click" && action != "double_click" &&
        action != "drag" && action != "scroll" && action != "key" && action != "type" && action != "list_screens" &&
        action != "select_screen" && action != "sign_in")
    {
        reply(false, "The action must be one of screenshot, move, click, double_click, drag, scroll, key, type, "
                     "list_screens, select_screen, sign_in.");
        return;
    }

    McpController::DesktopWindow* window = McpController::instance().desktopWindow(*client_id);
    if (!window)
    {
        if (hasClient(*client_id))
            reply(false, QString("The client %1 is not a desktop client.").arg(*client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    if (action == "list_screens")
    {
        const proto::screen::ScreenList screen_list = window->mcpScreenList();
        QJsonArray screens;

        for (int i = 0; i < screen_list.screen_size(); ++i)
        {
            const proto::screen::Screen& screen = screen_list.screen(i);
            screens.append(QJsonObject{ { "screen_id", static_cast<qint64>(screen.id()) },
                                        { "title", QString::fromStdString(screen.title()) },
                                        { "x", screen.position().x() },
                                        { "y", screen.position().y() },
                                        { "width", screen.resolution().width() },
                                        { "height", screen.resolution().height() },
                                        { "primary", screen.id() == screen_list.primary_screen() },
                                        { "current", screen.id() == screen_list.current_screen() } });
        }

        QJsonObject content{ { "screens", screens } };
        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
        return;
    }

    if (action == "select_screen")
    {
        std::optional<qint64> screen_id = integerArgument(arguments.value("screen_id"));
        const proto::screen::ScreenList screen_list = window->mcpScreenList();

        for (int i = 0; screen_id.has_value() && i < screen_list.screen_size(); ++i)
        {
            const proto::screen::Screen& screen = screen_list.screen(i);
            if (screen.id() == *screen_id)
            {
                window->mcpSelectScreen(screen);
                reply(true, QString("The client %1 switches to the screen %2. Take a screenshot to see it.")
                    .arg(*client_id).arg(*screen_id));
                return;
            }
        }

        reply(false, "The screen_id must be an id from list_screens.");
        return;
    }

    if (action == "sign_in")
    {
        const QString sign_in_error = signInError();
        if (!sign_in_error.isEmpty())
        {
            reply(false, sign_in_error);
            return;
        }

        switch (window->mcpSignIn())
        {
            case McpController::SignInResult::STARTED:
                reply(true, QString("The client %1 signs in to the host with the saved credentials. Take a "
                                    "screenshot to see the result.").arg(*client_id));
                break;
            case McpController::SignInResult::NOT_REQUESTED:
                reply(false, QString("The host of the client %1 does not ask for credentials now.").arg(*client_id));
                break;
            case McpController::SignInResult::NOT_SELECTED:
                reply(false, QString("No saved credentials are chosen for the host of the client %1, the user "
                                     "signs in.").arg(*client_id));
                break;
        }
        return;
    }

    const QSize frame_size = window->mcpFrameSize();
    if (frame_size.isEmpty())
    {
        reply(false, QString("The client %1 has no picture of the desktop yet.").arg(*client_id));
        return;
    }

    const QSize picture_size = pictureSize(frame_size);

    if (action == "screenshot")
    {
        QImage image = window->mcpScreenshot();
        if (image.isNull())
        {
            reply(false, QString("The client %1 has no picture of the desktop yet.").arg(*client_id));
            return;
        }

        if (picture_size != frame_size)
            image = image.scaled(picture_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QByteArray png;
        QBuffer buffer(&png);
        if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        {
            reply(false, "Unable to encode the picture.");
            return;
        }

        reply(true, QString("The desktop of the client %1 is %2x%3 pixels, the picture is %4x%5 pixels.")
            .arg(*client_id).arg(frame_size.width()).arg(frame_size.height())
            .arg(picture_size.width()).arg(picture_size.height()), png);
        return;
    }

    const QString done = QString("The client %1 got %2. Take a screenshot to see the result.")
        .arg(*client_id).arg(action);

    if (action == "key")
    {
        std::optional<QList<quint32>> usb_keycodes = keysArgument(arguments.value("keys"));
        if (!usb_keycodes.has_value())
        {
            reply(false, "The keys must be one key with its modifiers, for example ctrl+c. The description of keys "
                         "lists the names.");
            return;
        }

        for (quint32 usb_keycode : std::as_const(*usb_keycodes))
            window->mcpSendKeyEvent(usb_keycode, true);
        for (auto it = usb_keycodes->crbegin(); it != usb_keycodes->crend(); ++it)
            window->mcpSendKeyEvent(*it, false);

        reply(true, done);
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

        window->mcpSendText(text);
        reply(true, done);
        return;
    }

    std::optional<QPoint> pos = pointArgument(arguments, "x", "y", picture_size);
    if (!pos.has_value())
    {
        reply(false, QString("The x and y must be integers inside the picture of %1x%2 pixels.")
            .arg(picture_size.width()).arg(picture_size.height()));
        return;
    }

    auto send = [window, picture_size, frame_size](const QPoint& point, quint32 mask)
    {
        sendMouse(window, point, picture_size, frame_size, mask);
    };

    if (action == "move")
    {
        send(*pos, proto::input::MouseEvent::EMPTY);
    }
    else if (action == "scroll")
    {
        std::optional<quint32> wheel = wheelArgument(arguments.value("direction"));
        if (!wheel.has_value())
        {
            reply(false, "The direction of scroll must be one of up, down, left, right.");
            return;
        }

        std::optional<qint64> amount = kDefaultScrollAmount;
        if (arguments.contains("amount"))
            amount = integerArgument(arguments.value("amount"));

        if (!amount.has_value() || *amount < 1 || *amount > kMaxScrollAmount)
        {
            reply(false, QString("The amount must be an integer from 1 to %1.").arg(kMaxScrollAmount));
            return;
        }

        send(*pos, proto::input::MouseEvent::EMPTY);
        for (qint64 i = 0; i < *amount; ++i)
            send(*pos, *wheel);
    }
    else
    {
        std::optional<quint32> button = buttonArgument(arguments.value("button"));
        if (!button.has_value())
        {
            reply(false, "The button must be one of left, right, middle.");
            return;
        }

        if (action == "drag")
        {
            std::optional<QPoint> end = pointArgument(arguments, "to_x", "to_y", picture_size);
            if (!end.has_value())
            {
                reply(false, QString("The to_x and to_y must be integers inside the picture of %1x%2 pixels.")
                    .arg(picture_size.width()).arg(picture_size.height()));
                return;
            }

            send(*pos, proto::input::MouseEvent::EMPTY);
            send(*pos, *button);
            for (int i = 1; i <= kDragSteps; ++i)
                send(*pos + (*end - *pos) * i / kDragSteps, *button);
            send(*end, proto::input::MouseEvent::EMPTY);
        }
        else
        {
            send(*pos, proto::input::MouseEvent::EMPTY);
            send(*pos, *button);
            send(*pos, proto::input::MouseEvent::EMPTY);

            if (action == "double_click")
            {
                // The desktop of the host takes two clicks sent at once for a single click.
                QTimer::singleShot(kDoubleClickDelay, &McpController::instance(),
                    [client_id = *client_id, point = *pos, button = *button, picture_size, frame_size, done,
                     reply]()
                {
                    McpController::DesktopWindow* window = McpController::instance().desktopWindow(client_id);
                    if (!window)
                    {
                        reply(false, QString("The client %1 was closed before the second click.").arg(client_id));
                        return;
                    }

                    sendMouse(window, point, picture_size, frame_size, button);
                    sendMouse(window, point, picture_size, frame_size, proto::input::MouseEvent::EMPTY);
                    reply(true, done);
                });
                return;
            }
        }
    }

    reply(true, done);
}
