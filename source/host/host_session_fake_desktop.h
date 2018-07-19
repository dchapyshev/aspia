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

#ifndef ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H_
#define ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H_

#include "host/host_session_fake.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class DesktopFrame;
class VideoEncoder;

class HostSessionFakeDesktop : public HostSessionFake
{
    Q_OBJECT

public:
    explicit HostSessionFakeDesktop(QObject* parent);

    // HostSessionFake implementation.
    void startSession() override;

public slots:
    // HostSessionFake implementation.
    void onMessageReceived(const QByteArray& buffer) override;
    void onMessageWritten(int message_id) override;

private:
    std::unique_ptr<VideoEncoder> createEncoder(const proto::desktop::Config& config);
    std::unique_ptr<DesktopFrame> createFrame();
    void sendPacket(std::unique_ptr<proto::desktop::VideoPacket> packet);

    Q_DISABLE_COPY(HostSessionFakeDesktop)
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H_
