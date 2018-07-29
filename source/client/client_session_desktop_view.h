//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H_
#define ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H_

#include <QPointer>
#include <QThread>

#include "client/client_session.h"
#include "client/connect_data.h"
#include "codec/video_decoder.h"
#include "protocol/address_book.pb.h"

namespace aspia {

class DesktopWindow;

class ClientSessionDesktopView : public ClientSession
{
    Q_OBJECT

public:
    ClientSessionDesktopView(ConnectData* connect_data, QObject* parent);
    virtual ~ClientSessionDesktopView();

    static uint32_t supportedVideoEncodings();
    static uint32_t supportedFeatures();

public slots:
    // ClientSession implementation.
    void messageReceived(const QByteArray& buffer) override;
    void startSession() override;
    void closeSession() override;

    virtual void onSendConfig(const proto::desktop::Config& config);
    virtual void onSendScreen(const proto::desktop::Screen& screen);

protected:
    void readVideoPacket(const proto::desktop::VideoPacket& packet);
    void readScreenList(const proto::desktop::ScreenList& screen_list);

    ConnectData* connect_data_;
    QPointer<DesktopWindow> desktop_window_;

private:
    void readConfigRequest(const proto::desktop::ConfigRequest& config_request);

    proto::desktop::VideoEncoding video_encoding_ = proto::desktop::VIDEO_ENCODING_UNKNOWN;
    std::unique_ptr<VideoDecoder> video_decoder_;

    Q_DISABLE_COPY(ClientSessionDesktopView)
};

} // namespace aspia

#endif // ASPIA_CLIENT__CLIENT_SESSION_DESKTOP_VIEW_H_
