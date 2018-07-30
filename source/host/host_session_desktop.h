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

#ifndef ASPIA_HOST__HOST_SESSION_DESKTOP_H_
#define ASPIA_HOST__HOST_SESSION_DESKTOP_H_

#include "host/host_session.h"
#include "protocol/authorization.pb.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class Clipboard;
class InputInjector;
class ScreenUpdater;

class HostSessionDesktop : public HostSession
{
    Q_OBJECT

public:
    HostSessionDesktop(proto::auth::SessionType session_type, const QString& channel_id);
    ~HostSessionDesktop() = default;

public slots:
    // HostSession implementation.
    void messageReceived(const QByteArray& buffer) override;

protected:
    // HostSession implementation.
    void startSession() override;
    void stopSession() override;

private slots:
    void clipboardEvent(const proto::desktop::ClipboardEvent& event);

private:
    void readPointerEvent(const proto::desktop::PointerEvent& event);
    void readKeyEvent(const proto::desktop::KeyEvent& event);
    void readClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void readConfig(const proto::desktop::Config& config);
    void readScreen(const proto::desktop::Screen& screen);

    const proto::auth::SessionType session_type_;

    QPointer<ScreenUpdater> screen_updater_;
    QPointer<Clipboard> clipboard_;
    QScopedPointer<InputInjector> input_injector_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionDesktop);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_DESKTOP_H_
