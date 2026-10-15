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

#include "client/mcp/mcp_controller.h"

namespace {

const int kMaxImageSize = 1280;

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
                        { "enum", QJsonArray{ "screenshot" } },
                        { "description", "screenshot returns the picture of the remote desktop that the user "
                                         "sees in the window." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that is connected to the desktop of a host. "
                         "The window must be opened before, for example by connect_local_host or "
                         "connect_router_host with the desktop session_type, and its client_id comes from "
                         "list_clients. A picture larger than 1280 pixels on a side is scaled down." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void DesktopClientTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue id_value = arguments.value("client_id");
    if (!id_value.isDouble() || id_value.toDouble() != static_cast<double>(id_value.toInteger()))
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    if (arguments.value("action").toString() != "screenshot")
    {
        reply(false, "The action must be screenshot.");
        return;
    }

    const qint64 client_id = id_value.toInteger();

    McpController::DesktopWindow* window = McpController::instance().desktopWindow(client_id);
    if (!window)
    {
        if (hasClient(client_id))
            reply(false, QString("The client %1 is not a desktop client.").arg(client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(client_id));
        return;
    }

    QImage image = window->screenshot();
    if (image.isNull())
    {
        reply(false, QString("The client %1 has no picture of the desktop yet.").arg(client_id));
        return;
    }

    const QSize frame_size = image.size();
    if (image.width() > kMaxImageSize || image.height() > kMaxImageSize)
        image = image.scaled(kMaxImageSize, kMaxImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
    {
        reply(false, "Unable to encode the picture.");
        return;
    }

    reply(true, QString("The desktop of the client %1 is %2x%3 pixels, the picture is %4x%5 pixels.")
        .arg(client_id).arg(frame_size.width()).arg(frame_size.height()).arg(image.width()).arg(image.height()),
        png);
}
