//
// PROJECT:         Aspia
// FILE:            ipc/ipc_server.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_IPC__IPC_SERVER_H
#define _ASPIA_IPC__IPC_SERVER_H

#include <QPointer>

class QLocalServer;

namespace aspia {

class IpcChannel;

class IpcServer : public QObject
{
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer() = default;

    bool isStarted() const;

public slots:
    void start();
    void stop();

signals:
    void started(const QString& channel_id);
    void finished();
    void newConnection(IpcChannel* channel);
    void errorOccurred();

private slots:
    void onNewConnection();

private:
    QPointer<QLocalServer> server_;

    Q_DISABLE_COPY(IpcServer)
};

} // namespace aspia

#endif // _ASPIA_IPC__IPC_SERVER_H
