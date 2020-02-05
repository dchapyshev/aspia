//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__DESKTOP_SESSION_FAKE_H
#define HOST__DESKTOP_SESSION_FAKE_H

#include "base/macros_magic.h"
#include "host/desktop_session.h"

namespace desktop {
class SharedFrame;
} // namespace desktop

namespace host {

class DesktopSessionFake : public DesktopSession
{
public:
    explicit DesktopSessionFake(Delegate* delegate);
    ~DesktopSessionFake();

    // DesktopSession implementation.
    void start() override;
    void startSession() override;
    void stopSession() override;
    void selectScreen(const proto::Screen& screen) override;
    void setFeatures(const proto::internal::SetFeatures& features) override;
    void injectKeyEvent(const proto::KeyEvent& event) override;
    void injectPointerEvent(const proto::PointerEvent& event) override;
    void injectClipboardEvent(const proto::ClipboardEvent& event) override;
    void logoffUserSession() override;
    void lockUserSession() override;

private:
    void nextBoxColor();

    enum BoxColor : uint32_t
    {
        kWhite = 0xFFFFFFFF,
        kRed   = 0xFF000000,
        kGreen = 0x00800000,
        kBlue  = 0x0000FF00
    };

    static const int kQueueLength = 4;

    BoxColor color_queue_[kQueueLength] = { kWhite, kRed, kGreen, kBlue };
    int color_index_ = 0;

    int box_pos_x_ = 0;
    int box_pos_y_ = 0;
    int box_speed_x_;
    int box_speed_y_;

    uint32_t prev_pointer_mask_ = 0;

    std::unique_ptr<desktop::SharedFrame> frame_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(DesktopSessionFake);
};

} // namespace host

#endif // HOST__DESKTOP_SESSION_FAKE_H
