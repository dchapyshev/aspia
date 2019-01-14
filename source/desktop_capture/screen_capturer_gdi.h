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

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H
#define ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H

#include "base/win/scoped_hdc.h"
#include "desktop_capture/win/scoped_thread_desktop.h"
#include "desktop_capture/screen_capturer.h"
#include "desktop_capture/screen_capture_frame_queue.h"

namespace desktop {

class Differ;

class ScreenCapturerGDI : public ScreenCapturer
{
public:
    ScreenCapturerGDI();
    ~ScreenCapturerGDI();

    int screenCount() override;
    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;

    const DesktopFrame* captureFrame() override;

private:
    bool prepareCaptureResources();

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    QString current_device_key_;

    ScopedThreadDesktop desktop_;
    QRect desktop_dc_rect_;

    std::unique_ptr<Differ> differ_;
    std::unique_ptr<base::win::ScopedGetDC> desktop_dc_;
    base::win::ScopedCreateDC memory_dc_;

    ScreenCaptureFrameQueue queue_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerGDI);
};

} // namespace desktop

#endif // ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_GDI_H
