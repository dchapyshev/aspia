//
// PROJECT:         Aspia
// FILE:            host/win/host.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__WIN__HOST_H
#define _ASPIA_HOST__WIN__HOST_H

#include <QObject>
#include <QPointer>
#include <QQueue>

#include "protocol/authorization.pb.h"

namespace aspia {

class HostProcess;
class IpcChannel;
class IpcServer;
class NetworkChannel;

class Host : public QObject
{
    Q_OBJECT

public:
    enum State
    {
        StoppedState,
        StartingState,
        DetachedState,
        AttachedState
    };
    Q_ENUM(State);

    Host(QObject* parent = nullptr);
    ~Host();

    NetworkChannel* networkChannel() const { return network_channel_; }
    void setNetworkChannel(NetworkChannel* network_channel);

    proto::auth::SessionType sessionType() const { return session_type_; }
    void setSessionType(proto::auth::SessionType session_type);

    QString userName() const { return user_name_; }
    void setUserName(const QString& user_name);

    QString uuid() const { return uuid_; }
    void setUuid(const QString& uuid);

    QString remoteAddress() const;

    bool start();

public slots:
    void stop();
    void sessionChanged(quint32 event, quint32 session_id);

signals:
    void finished(Host* host);

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private slots:
    void networkMessageWritten(int message_id);
    void networkMessageReceived(const QByteArray& buffer);
    void ipcMessageWritten(int message_id);
    void ipcMessageReceived(const QByteArray& buffer);
    void ipcServerStarted(const QString& channel_id);
    void ipcNewConnection(IpcChannel* channel);
    void attachSession(quint32 session_id);
    void dettachSession();

private:
    static const quint32 kInvalidSessionId = 0xFFFFFFFF;

    proto::auth::SessionType session_type_ = proto::auth::SESSION_TYPE_UNKNOWN;
    QString user_name_;
    QString uuid_;

    quint32 session_id_ = kInvalidSessionId;
    int attach_timer_id_ = 0;
    State state_ = StoppedState;

    QPointer<NetworkChannel> network_channel_;
    QPointer<IpcChannel> ipc_channel_;
    QPointer<HostProcess> session_process_;

    Q_DISABLE_COPY(Host)
};

} // namespace aspia

#endif // _ASPIA_HOST__WIN__HOST_H
