//
// PROJECT:         Aspia
// FILE:            host/host_notifier.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_NOTIFIER_H
#define _ASPIA_HOST__HOST_NOTIFIER_H

#include <QObject>

#include "host/ui/host_notifier_window.h"
#include "ipc/ipc_channel.h"

namespace aspia {

class HostNotifier : public QObject
{
    Q_OBJECT

public:
    explicit HostNotifier(QObject* parent = nullptr);
    ~HostNotifier();

    QString channelId() const;
    void setChannelId(const QString& channel_id);

    bool start();

signals:
    void sessionClose(const std::string& uuid);

private slots:
    void stop();
    void killSession(const std::string& uuid);
    void onIpcChannelConnected();
    void onIpcMessageReceived(const QByteArray& buffer);

private:
    QPointer<IpcChannel> ipc_channel_;
    QPointer<HostNotifierWindow> window_;
    QString channel_id_;

    Q_DISABLE_COPY(HostNotifier)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_NOTIFIER_H
