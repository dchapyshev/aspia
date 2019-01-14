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

#ifndef ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H
#define ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H

#include "base/macros_magic.h"
#include "host/host_session_fake.h"
#include "proto/desktop_session.pb.h"

namespace codec {
class VideoEncoder;
} // namespace codec

namespace desktop {
class DesktopFrame;
} // namespace desktop

namespace host {

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

private:
    codec::VideoEncoder* createEncoder(const proto::desktop::Config& config);
    std::unique_ptr<desktop::DesktopFrame> createFrame();

    DISALLOW_COPY_AND_ASSIGN(HostSessionFakeDesktop);
};

} // namespace host

#endif // ASPIA_HOST__HOST_SESSION_FAKE_DESKTOP_H
