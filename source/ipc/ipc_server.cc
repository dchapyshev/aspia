//
// PROJECT:         Aspia
// FILE:            ipc/ipc_server.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/ipc_server.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocalServer>

#include "crypto/random.h"
#include "ipc/ipc_channel.h"

namespace aspia {

namespace {

QString generateUniqueChannelId()
{
    static std::atomic_uint32_t last_channel_id = 0;
    quint32 channel_id = last_channel_id++;

    return QString("%1.%2.%3")
        .arg(QCoreApplication::applicationPid())
        .arg(channel_id)
        .arg(Random::generateNumber());
}

} // namespace

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

IpcServer::~IpcServer() = default;

bool IpcServer::isStarted() const
{
    return !server_.isNull();
}

void IpcServer::start()
{
    if (isStarted())
    {
        qWarning("An attempt was start an already running server.");
        return;
    }

    server_ = new QLocalServer(this);

    server_->setSocketOptions(QLocalServer::OtherAccessOption);
    server_->setMaxPendingConnections(1);

    connect(server_, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);

    QString channel_id = generateUniqueChannelId();

    if (!server_->listen(channel_id))
    {
        qWarning() << "listen failed: " << server_->errorString();
        emit errorOccurred();
        stop();
        return;
    }

    emit started(channel_id);
}

void IpcServer::stop()
{
    if (!server_.isNull())
    {
        server_->close();
        delete server_;
        emit finished();
    }
}

void IpcServer::onNewConnection()
{
    if (server_->hasPendingConnections())
    {
        QLocalSocket* socket = server_->nextPendingConnection();
        emit newConnection(new IpcChannel(socket, nullptr));
        emit finished();
    }
}

} // namespace aspia
