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

#include "host/desktop_config_tracker.h"
#include "host/host_session.h"
#include "protocol/session_type.pb.h"
#include "build/build_config.h"

namespace aspia {

class Clipboard;
class InputInjector;
class ScreenUpdater;
class VisualEffectsDisabler;

class HostSessionDesktop : public HostSession
{
    Q_OBJECT

public:
    HostSessionDesktop(proto::SessionType session_type, const QString& channel_id);
    ~HostSessionDesktop();

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
    void readPowerControl(const proto::desktop::PowerControl& power_control);
    void readConfig(const proto::desktop::Config& config);
    void readScreen(const proto::desktop::Screen& screen);

    const proto::SessionType session_type_;

    proto::desktop::ClientToHost incoming_message_;
    proto::desktop::HostToClient outgoing_message_;

    DesktopConfigTracker config_tracker_;

    QPointer<ScreenUpdater> screen_updater_;
    QPointer<Clipboard> clipboard_;
    QScopedPointer<InputInjector> input_injector_;

#if defined(OS_WIN)
    std::unique_ptr<VisualEffectsDisabler> effects_disabler_;
#endif // defined(OS_WIN)

    DISALLOW_COPY_AND_ASSIGN(HostSessionDesktop);
};

} // namespace aspia

#endif // ASPIA_HOST__HOST_SESSION_DESKTOP_H_
