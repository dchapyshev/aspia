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

#include <optional>

#include "client/mcp/mcp_controller.h"
#include "proto/desktop_input.h"

namespace {

const int kMaxPictureSize = 1280;
const int kDragSteps = 10;
const int kDefaultScrollAmount = 3;
const int kMaxScrollAmount = 20;

//--------------------------------------------------------------------------------------------------
bool hasClient(qint64 client_id)
{
    const QList<McpController::ClientInfo> clients = McpController::instance().clients();
    for (const McpController::ClientInfo& client : clients)
    {
        if (client.client_id == client_id)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
QSize pictureSize(const QSize& frame_size)
{
    if (frame_size.width() <= kMaxPictureSize && frame_size.height() <= kMaxPictureSize)
        return frame_size;

    return frame_size.scaled(kMaxPictureSize, kMaxPictureSize, Qt::KeepAspectRatio);
}

//--------------------------------------------------------------------------------------------------
std::optional<qint64> integerArgument(const QJsonValue& value)
{
    if (!value.isDouble() || value.toDouble() != static_cast<double>(value.toInteger()))
        return std::nullopt;

    return value.toInteger();
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
                        { "enum", QJsonArray{ "screenshot", "move", "click", "double_click", "drag", "scroll" } },
                        { "description", "screenshot returns the picture of the remote desktop that the user "
                                         "sees in the window. move, click, double_click and scroll act at x, y. "
                                         "drag presses the button at x, y, moves to to_x, to_y and releases the "
                                         "button there." } };

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

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that is connected to the desktop of a host. "
                         "The window must be opened before, for example by connect_local_host or "
                         "connect_router_host with the desktop session_type, and its client_id comes from "
                         "list_clients. A picture larger than 1280 pixels on a side is scaled down. The mouse "
                         "actions take the position in the pixels of that picture and return no picture, so take "
                         "a screenshot to see the result. The user sees every action in the window." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "x", x },
                                                                   { "y", y },
                                                                   { "to_x", to_x },
                                                                   { "to_y", to_y },
                                                                   { "button", button },
                                                                   { "direction", direction },
                                                                   { "amount", amount } } },
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

    const QString action = arguments.value("action").toString();
    if (action != "screenshot" && action != "move" && action != "click" && action != "double_click" &&
        action != "drag" && action != "scroll")
    {
        reply(false, "The action must be one of screenshot, move, click, double_click, drag, scroll.");
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

    const QSize frame_size = window->frameSize();
    if (frame_size.isEmpty())
    {
        reply(false, QString("The client %1 has no picture of the desktop yet.").arg(*client_id));
        return;
    }

    const QSize picture_size = pictureSize(frame_size);

    if (action == "screenshot")
    {
        QImage image = window->screenshot();
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

    std::optional<QPoint> pos = pointArgument(arguments, "x", "y", picture_size);
    if (!pos.has_value())
    {
        reply(false, QString("The x and y must be integers inside the picture of %1x%2 pixels.")
            .arg(picture_size.width()).arg(picture_size.height()));
        return;
    }

    auto send = [window, picture_size, frame_size](const QPoint& point, quint32 mask)
    {
        proto::input::MouseEvent event;
        event.set_x(point.x() * frame_size.width() / picture_size.width());
        event.set_y(point.y() * frame_size.height() / picture_size.height());
        event.set_mask(mask);
        window->sendMouseEvent(event);
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
            const int clicks = (action == "double_click") ? 2 : 1;

            send(*pos, proto::input::MouseEvent::EMPTY);
            for (int i = 0; i < clicks; ++i)
            {
                send(*pos, *button);
                send(*pos, proto::input::MouseEvent::EMPTY);
            }
        }
    }

    reply(true, QString("The client %1 got %2. Take a screenshot to see the result.").arg(*client_id).arg(action));
}
