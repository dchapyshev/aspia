//
// PROJECT:         Aspia
// FILE:            host/host_server.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SERVER_H
#define _ASPIA_HOST__HOST_SERVER_H

#include "host/win/host_process.h"
#include "host/user.h"
#include "ipc/ipc_channel.h"
#include "network/network_server.h"

namespace aspia {

class Host;
class HostUserAuthorizer;

class HostServer : public QObject
{
    Q_OBJECT

public:
    HostServer(QObject* parent = nullptr);
    ~HostServer();

    bool start(int port, const QList<User>& user_list);
    void stop();
    void setSessionChanged(quint32 event, quint32 session_id);

signals:
    void sessionChanged(quint32 event, quint32 session_id);

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void onNewConnection();
    void onAuthorizationFinished(HostUserAuthorizer* authorizer);
    void onHostFinished(Host* host);
    void onIpcServerStarted(const QString& channel_id);
    void onIpcNewConnection(IpcChannel* channel);
    void onIpcMessageReceived(const QByteArray& buffer);
    void onNotifierProcessError(HostProcess::ErrorCode error_code);
    void restartNotifier();

private:
    enum class NotifierState
    {
        Stopped,
        Starting,
        Started
    };

    void startNotifier();
    void stopNotifier();
    void sessionToNotifier(const Host& host);
    void sessionCloseToNotifier(const Host& host);

    // Accepts incoming network connections.
    QPointer<NetworkServer> network_server_;

    // Contains the status of the notifier process.
    NotifierState notifier_state_ = NotifierState::Stopped;

    // Starts and monitors the status of the notifier process.
    QPointer<HostProcess> notifier_process_;

    // The channel is used to communicate with the notifier process.
    QPointer<IpcChannel> ipc_channel_;

    // Contains a list of users for authorization.
    QList<User> user_list_;

    // Contains a list of connected sessions.
    QList<QPointer<Host>> session_list_;

    int restart_timer_id_ = 0;

    Q_DISABLE_COPY(HostServer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SERVER_H
