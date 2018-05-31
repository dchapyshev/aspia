//
// PROJECT:         Aspia
// FILE:            host/host_notifier.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_NOTIFIER_H
#define _ASPIA_HOST__HOST_NOTIFIER_H

#include "ipc/ipc_channel.h"
#include "protocol/notifier.pb.h"

namespace aspia {

class HostNotifier : public QObject
{
    Q_OBJECT

public:
    explicit HostNotifier(QObject* parent = nullptr);
    ~HostNotifier();

    bool start(const QString& channel_id);

public slots:
    void stop();
    void killSession(const std::string& uuid);

signals:
    void finished();
    void sessionOpen(const proto::notifier::Session& session);
    void sessionClose(const proto::notifier::SessionClose& session_close);

private slots:
    void onIpcChannelConnected();
    void onIpcMessageReceived(const QByteArray& buffer);

private:
    QPointer<IpcChannel> ipc_channel_;

    Q_DISABLE_COPY(HostNotifier)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_NOTIFIER_H
