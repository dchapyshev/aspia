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

#ifndef DESKTOP__SCREEN_CAPTURER_GDI_H
#define DESKTOP__SCREEN_CAPTURER_GDI_H

#include "base/win/scoped_hdc.h"
#include "desktop/screen_capturer.h"
#include "desktop/shared_frame.h"

namespace ipc {
class SharedMemoryFactory;
} // namespace ipc

namespace desktop {

class Differ;

class ScreenCapturerGdi : public ScreenCapturer
{
public:
    ScreenCapturerGdi();
    ~ScreenCapturerGdi();

    // ScreenCapturer implementation.
    int screenCount() override;
    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;
    const Frame* captureFrame(Error* error) override;

protected:
    // ScreenCapturer implementation.
    void reset() override;

private:
    const Frame* captureImage();
    bool prepareCaptureResources();

    bool composition_changed_ = false;

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    std::wstring current_device_key_;

    Rect desktop_dc_rect_;
    PixelFormat pixel_format_;

    std::unique_ptr<Differ> differ_;
    base::win::ScopedGetDC desktop_dc_;
    base::win::ScopedCreateDC memory_dc_;

    FrameQueue<Frame> queue_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerGdi);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURER_GDI_H
