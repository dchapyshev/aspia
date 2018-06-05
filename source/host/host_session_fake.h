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
#include "protocol/desktop_session.pb.h"

namespace aspia {

class DesktopFrame;
class VideoEncoder;

class HostSessionFake : public QObject
{
    Q_OBJECT

public:
    ~HostSessionFake() = default;

    static HostSessionFake* create(proto::auth::SessionType session_type, QObject* parent);

    void startSession();

signals:
    void writeMessage(int message_id, const QByteArray& buffer);
    void readMessage();
    void errorOccurred();

public slots:
    void onMessageReceived(const QByteArray& buffer);

protected:
    explicit HostSessionFake(QObject* parent);

private:
    std::unique_ptr<VideoEncoder> createEncoder(const proto::desktop::Config& config);
    std::unique_ptr<DesktopFrame> createFrame();
    void sendPacket(std::unique_ptr<proto::desktop::VideoPacket> packet);

    Q_DISABLE_COPY(HostSessionFake)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_FAKE_H
