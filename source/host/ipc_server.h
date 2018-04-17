//
// PROJECT:         Aspia
// FILE:            host/ipc_server.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__IPC_SERVER_H
#define _ASPIA_HOST__IPC_SERVER_H

#include <QObject>
#include <QPointer>

class QLocalServer;

namespace aspia {

class IpcChannel;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    IpcServer(QObject* parent = nullptr);
    ~IpcServer();

    bool isStarted() const;

public slots:
    void start();
    void stop();

signals:
    void started(const QString& channel_id);
    void finished();
    void newConnection(IpcChannel* channel);
    void errorOccurred();

private:
    QPointer<QLocalServer> server_;

    Q_DISABLE_COPY(IpcServer)
};

} // namespace aspia

#endif // _ASPIA_HOST__IPC_SERVER_H
