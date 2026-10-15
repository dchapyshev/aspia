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

#include "client/mcp/mcp_worker.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#else
#include <unistd.h>
#endif

#include <cerrno>
#include <cstdio>

#include "version.h"

namespace {

const char kModernVersion[] = "2026-07-28";
const char kVersionKey[] = "io.modelcontextprotocol/protocolVersion";
const char kCapabilitiesKey[] = "io.modelcontextprotocol/clientCapabilities";
const char kServerInfoKey[] = "io.modelcontextprotocol/serverInfo";

// Revisions with the initialize handshake, the latest first.
const QStringList kLegacyVersions = { "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05" };

constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kUnsupportedProtocolVersion = -32022;

//--------------------------------------------------------------------------------------------------
QString channelName()
{
    quint32 id = 0;
#if defined(Q_OS_WINDOWS)
    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
        PLOG(ERROR) << "ProcessIdToSessionId failed";
    id = session_id;
#else
    id = getuid();
#endif
    return "aspia_client_mcp_" + QString::number(id);
}

//--------------------------------------------------------------------------------------------------
qint64 readInput(char* buffer, qint64 size)
{
    qint64 result = 0;
#if defined(Q_OS_WINDOWS)
    DWORD read_bytes = 0;
    if (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, static_cast<DWORD>(size), &read_bytes, nullptr))
        result = read_bytes;
#else
    do
    {
        result = read(STDIN_FILENO, buffer, static_cast<size_t>(size));
    }
    while (result < 0 && errno == EINTR);
#endif
    return result;
}

//--------------------------------------------------------------------------------------------------
bool writeOutput(const QByteArray& data)
{
    const char* buffer = data.constData();
    qint64 remaining = data.size();

    while (remaining > 0)
    {
        qint64 written = 0;
#if defined(Q_OS_WINDOWS)
        DWORD written_bytes = 0;
        if (WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, static_cast<DWORD>(remaining), &written_bytes, nullptr))
            written = written_bytes;
#else
        written = write(STDOUT_FILENO, buffer, static_cast<size_t>(remaining));
        if (written < 0 && errno == EINTR)
            continue;
#endif
        if (written <= 0)
            return false;

        buffer += written;
        remaining -= written;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QJsonObject serverInfo()
{
    return QJsonObject{ { "name", "aspia" }, { "version", ASPIA_VERSION_STRING } };
}

//--------------------------------------------------------------------------------------------------
QJsonObject capabilities()
{
    return QJsonObject{ { "tools", QJsonObject() } };
}

//--------------------------------------------------------------------------------------------------
QJsonArray supportedVersions()
{
    QJsonArray versions = QJsonArray::fromStringList(kLegacyVersions);
    versions.prepend(kModernVersion);
    return versions;
}

//--------------------------------------------------------------------------------------------------
QJsonObject resultResponse(const QJsonValue& id, const QJsonObject& result)
{
    return QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "result", result } };
}

//--------------------------------------------------------------------------------------------------
QJsonObject errorResponse(const QJsonValue& id, int code, const QString& message,
                          const QJsonObject& data = QJsonObject())
{
    QJsonObject error{ { "code", code }, { "message", message } };
    if (!data.isEmpty())
        error.insert("data", data);

    return QJsonObject{ { "jsonrpc", "2.0" }, { "id", id }, { "error", error } };
}

//--------------------------------------------------------------------------------------------------
QJsonObject completeResult(QJsonObject result, bool modern)
{
    if (modern)
    {
        result.insert("resultType", "complete");
        result.insert("_meta", QJsonObject{ { kServerInfoKey, serverInfo() } });
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
McpWorker::McpWorker()
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
McpWorker::~McpWorker()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
int McpWorker::runBridge()
{
    LOG(INFO) << "Starting MCP bridge";

    QLocalSocket socket;
    socket.connectToServer(channelName());

    static const MilliSeconds kConnectTimeout(5000);
    if (!socket.waitForConnected(static_cast<int>(kConnectTimeout.count())))
    {
        LOG(ERROR) << "Unable to connect to the client:" << socket.errorString();
        std::fputs("Aspia Client is not running or its MCP connector is turned off.\n", stderr);
        return 1;
    }

    connect(&socket, &QLocalSocket::readyRead, &socket, [&socket]()
    {
        if (!writeOutput(socket.readAll()))
            QCoreApplication::quit();
    });
    connect(&socket, &QLocalSocket::disconnected, QCoreApplication::instance(), &QCoreApplication::quit);

    struct Input
    {
        std::mutex lock;
        QLocalSocket* socket = nullptr;
    };

    std::shared_ptr<Input> input = std::make_shared<Input>();
    input->socket = &socket;

    // Nothing can interrupt the read of the standard input, so the thread is not joined and the
    // socket is reached only while it is alive.
    std::thread([input]()
    {
        char buffer[4096];
        qint64 size = 0;

        while ((size = readInput(buffer, sizeof(buffer))) > 0)
        {
            QByteArray data(buffer, size);

            std::scoped_lock lock(input->lock);
            if (!input->socket)
                return;

            QMetaObject::invokeMethod(input->socket, [socket = input->socket, data]()
            {
                socket->write(data);
            }, Qt::QueuedConnection);
        }

        std::scoped_lock lock(input->lock);
        if (input->socket)
        {
            QMetaObject::invokeMethod(input->socket, [socket = input->socket]()
            {
                socket->disconnectFromServer();
            }, Qt::QueuedConnection);
        }
    }).detach();

    int exit_code = QCoreApplication::exec();

    std::scoped_lock lock(input->lock);
    input->socket = nullptr;

    LOG(INFO) << "MCP bridge finished";
    return exit_code;
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onEnable()
{
    QString channel_name = channelName();

    // A socket file left by a crashed client prevents listening on UNIX.
    QLocalServer::removeServer(channel_name);

    if (!server_->listen(channel_name))
    {
        LOG(ERROR) << "Unable to listen on channel" << channel_name << ":" << server_->errorString();
        emit sig_enableFailed();
        return;
    }

    LOG(INFO) << "MCP connector started";
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onDisable()
{
    if (!server_->isListening())
        return;

    server_->close();

    const QList<QLocalSocket*> sockets = connections_.keys();
    connections_.clear();
    pending_.clear();

    for (QLocalSocket* socket : sockets)
    {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }

    LOG(INFO) << "MCP connector stopped";
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onToolsListed(qint64 request_id, const QJsonArray& tools)
{
    Pending pending = pending_.take(request_id);
    if (!pending.respond)
        return;

    QJsonObject result{ { "tools", tools } };
    if (pending.modern)
    {
        result.insert("ttlMs", 0);
        result.insert("cacheScope", "public");
    }

    pending.respond(resultResponse(pending.id, completeResult(result, pending.modern)));
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onToolFinished(qint64 request_id, bool success, const QString& text)
{
    Pending pending = pending_.take(request_id);
    if (!pending.respond)
        return;

    QJsonObject result{ { "content", QJsonArray{ QJsonObject{ { "type", "text" }, { "text", text } } } },
                        { "isError", !success } };
    pending.respond(resultResponse(pending.id, completeResult(result, pending.modern)));
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onToolNotFound(qint64 request_id, const QString& name)
{
    Pending pending = pending_.take(request_id);
    if (!pending.respond)
        return;

    pending.respond(errorResponse(pending.id, kInvalidParams, "Unknown tool: " + name));
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onPrepare()
{
    server_.reset(new QLocalServer(this));
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    connect(server_, &QLocalServer::newConnection, this, &McpWorker::onNewConnection);
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onStop()
{
    onDisable();

    server_.reset();
}

//--------------------------------------------------------------------------------------------------
void McpWorker::onNewConnection()
{
    while (QLocalSocket* socket = server_->nextPendingConnection())
    {
        LOG(INFO) << "MCP connection opened";
        connections_.insert(socket, Connection());

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() { readMessages(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]()
        {
            LOG(INFO) << "MCP connection closed";
            connections_.remove(socket);
            socket->deleteLater();
        });
    }
}

//--------------------------------------------------------------------------------------------------
void McpWorker::readMessages(QLocalSocket* socket)
{
    Connection& connection = connections_[socket];
    connection.buffer.append(socket->readAll());

    // A tool may answer after the connection is closed and the socket is deleted.
    Respond respond = [socket = QPointer<QLocalSocket>(socket)](const QJsonObject& response)
    {
        if (socket)
            socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
    };

    qsizetype end = connection.buffer.indexOf('\n');
    while (end != -1)
    {
        processMessage(connection.buffer.left(end), &connection, respond);
        connection.buffer.remove(0, end + 1);

        end = connection.buffer.indexOf('\n');
    }

    static const qsizetype kMaxMessageSize = 4 * 1024 * 1024;
    if (connection.buffer.size() > kMaxMessageSize)
    {
        LOG(ERROR) << "MCP message is too large";
        socket->abort();
    }
}

//--------------------------------------------------------------------------------------------------
// Messages that are not answered do not call |respond|.
void McpWorker::processMessage(const QByteArray& message, Connection* connection, const Respond& respond)
{
    if (message.trimmed().isEmpty())
        return;

    QJsonParseError parse_error;
    QJsonDocument document = QJsonDocument::fromJson(message, &parse_error);
    if (parse_error.error != QJsonParseError::NoError)
    {
        respond(errorResponse(QJsonValue::Null, kParseError, "Parse error"));
        return;
    }

    if (!document.isObject())
    {
        respond(errorResponse(QJsonValue::Null, kInvalidRequest, "Invalid request"));
        return;
    }

    QJsonObject request = document.object();

    // Notifications and responses are not answered.
    if (!request.contains("id") || request.contains("result") || request.contains("error"))
        return;

    QJsonValue id = request.value("id");
    QString method = request.value("method").toString();
    if (method.isEmpty())
    {
        respond(errorResponse(id, kInvalidRequest, "Invalid request"));
        return;
    }

    QJsonObject params = request.value("params").toObject();

    if (method == "initialize")
    {
        QString version = params.value("protocolVersion").toString();
        if (!kLegacyVersions.contains(version))
            version = kLegacyVersions.front();

        connection->initialized = true;
        respond(resultResponse(id, QJsonObject{
            { "protocolVersion", version }, { "capabilities", capabilities() }, { "serverInfo", serverInfo() } }));
        return;
    }

    QJsonObject meta = params.value("_meta").toObject();
    if (!meta.contains(kVersionKey))
    {
        // Requests of the revisions with the initialize handshake carry no version.
        if (!connection->initialized)
            respond(errorResponse(id, kInvalidParams, "Missing protocol version"));
        else
            processMethod(id, method, params, false, respond);
        return;
    }

    QString version = meta.value(kVersionKey).toString();
    if (version != kModernVersion)
    {
        respond(errorResponse(id, kUnsupportedProtocolVersion, "Unsupported protocol version",
                              QJsonObject{ { "supported", supportedVersions() }, { "requested", version } }));
        return;
    }

    if (!meta.contains(kCapabilitiesKey))
    {
        respond(errorResponse(id, kInvalidParams, "Missing client capabilities"));
        return;
    }

    processMethod(id, method, params, true, respond);
}

//--------------------------------------------------------------------------------------------------
void McpWorker::processMethod(const QJsonValue& id, const QString& method, const QJsonObject& params, bool modern,
                              const Respond& respond)
{
    if (method == "tools/list" || method == "tools/call")
    {
        const qint64 request_id = ++last_request_id_;
        pending_.insert(request_id, Pending{ id, modern, respond });

        QString name = params.value("name").toString();
        if (method == "tools/list")
            emit sig_toolsListRequested(request_id);
        else
            emit sig_toolCallRequested(request_id, name, params.value("arguments").toObject());
        return;
    }

    QJsonObject result;

    if (method == "server/discover")
    {
        result.insert("supportedVersions", supportedVersions());
        result.insert("capabilities", capabilities());

        if (modern)
        {
            result.insert("ttlMs", 0);
            result.insert("cacheScope", "public");
        }
    }
    else if (method != "ping")
    {
        respond(errorResponse(id, kMethodNotFound, "Method not found: " + method));
        return;
    }

    respond(resultResponse(id, completeResult(result, modern)));
}
