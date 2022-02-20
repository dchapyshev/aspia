//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_DESKTOP_SESSION_FAKE_H
#define HOST_DESKTOP_SESSION_FAKE_H

#include "base/macros_magic.h"
#include "host/desktop_session.h"

namespace base {
class TaskRunner;
} // namespace base

namespace host {

class DesktopSessionFake : public DesktopSession
{
public:
    DesktopSessionFake(std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate);
    ~DesktopSessionFake() override;

    // DesktopSession implementation.
    void start() override;
    void stop() override;
    void control(proto::internal::DesktopControl::Action action) override;
    void configure(const Config& config) override;
    void selectScreen(const proto::Screen& screen) override;
    void captureScreen() override;
    void injectKeyEvent(const proto::KeyEvent& event) override;
    void injectTextEvent(const proto::TextEvent& event) override;
    void injectMouseEvent(const proto::MouseEvent& event) override;
    void injectClipboardEvent(const proto::ClipboardEvent& event) override;

private:
    class FrameGenerator;
    std::shared_ptr<FrameGenerator> frame_generator_;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionFake);
};

} // namespace host

#endif // HOST_DESKTOP_SESSION_FAKE_H
