//
// PROJECT:         Aspia
// FILE:            ipc/ipc_server.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ipc/ipc_server.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocalServer>

#include <random>

#include "ipc/ipc_channel.h"

namespace aspia {

namespace {

QString generateUniqueChannelId()
{
    static std::atomic_uint32_t last_channel_id = 0;
    quint32 channel_id = last_channel_id++;

    std::random_device device;
    std::mt19937 enigne(device());
    std::uniform_int_distribution<quint32> dist(0, std::numeric_limits<quint32>::max());

    return QString("%1.%2.%3")
        .arg(QCoreApplication::applicationPid())
        .arg(channel_id)
        .arg(dist(enigne));
}

} // namespace

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

IpcServer::~IpcServer()
{
    stop();
}

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

    connect(server_, &QLocalServer::newConnection, [this]()
    {
        if (server_->hasPendingConnections())
        {
            QLocalSocket* socket = server_->nextPendingConnection();
            emit newConnection(new IpcChannel(socket, nullptr));
            stop();
        }
    });

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
    }

    emit finished();
}

} // namespace aspia
