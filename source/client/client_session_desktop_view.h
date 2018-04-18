//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_view.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H
#define _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H

#include <QPointer>
#include <QThread>

#include "client/client_session.h"
#include "codec/video_decoder.h"
#include "protocol/computer.pb.h"

namespace aspia {

class DesktopWindow;

class ClientSessionDesktopView : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionDesktopView(proto::Computer* computer, QObject* parent);
    virtual ~ClientSessionDesktopView();

public slots:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void messageWritten(int message_id) override;
    void startSession() override;
    void closeSession() override;

    virtual void onSendConfig(const proto::desktop::Config& config);

protected:
    void readVideoPacket(const proto::desktop::VideoPacket& packet);

    proto::Computer* computer_;
    QPointer<DesktopWindow> desktop_window_;

private:
    void readConfigRequest(const proto::desktop::ConfigRequest& config_request);

    proto::desktop::VideoEncoding video_encoding_ = proto::desktop::VIDEO_ENCODING_UNKNOWN;
    std::unique_ptr<VideoDecoder> video_decoder_;

    Q_DISABLE_COPY(ClientSessionDesktopView)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H
