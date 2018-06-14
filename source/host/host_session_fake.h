//
// PROJECT:         Aspia
// FILE:            host/host_session_fake.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_FAKE_H
#define _ASPIA_HOST__HOST_SESSION_FAKE_H

#include <QObject>

#include "protocol/authorization.pb.h"

namespace aspia {

class HostSessionFake : public QObject
{
    Q_OBJECT

public:
    virtual ~HostSessionFake() = default;

    static HostSessionFake* create(proto::auth::SessionType session_type, QObject* parent);

    virtual void startSession() = 0;

signals:
    void writeMessage(int message_id, const QByteArray& buffer);
    void readMessage();
    void errorOccurred();

public slots:
    virtual void onMessageReceived(const QByteArray& buffer) = 0;
    virtual void onMessageWritten(int message_id) = 0;

protected:
    explicit HostSessionFake(QObject* parent);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FAKE_H
