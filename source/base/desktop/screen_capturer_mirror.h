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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_MIRROR_H
#define BASE_DESKTOP_SCREEN_CAPTURER_MIRROR_H

#include "base/desktop/screen_capturer.h"
#include "base/win/scoped_hdc.h"

namespace base {

class MirrorHelper;
class SharedMemoryFactory;

class ScreenCapturerMirror : public ScreenCapturer
{
public:
    ScreenCapturerMirror();
    ~ScreenCapturerMirror() override;

    bool isSupported();

    // ScreenCapturer implementation.
    int screenCount() override;
    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;
    ScreenId currentScreen() const override;
    const Frame* captureFrame(Error* error) override;
    const MouseCursor* captureCursor() override;
    Point cursorPosition() override;

protected:
    // ScreenCapturer implementation.
    void reset() override;

private:
    void updateExcludeRegion();
    Error prepareCaptureResources();

    std::unique_ptr<MirrorHelper> helper_;
    std::unique_ptr<Frame> frame_;

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    std::wstring current_device_key_;

    Rect desktop_rect_;
    Region exclude_region_;

    win::ScopedGetDC desktop_dc_;
    std::unique_ptr<MouseCursor> mouse_cursor_;
    Point last_dpi_;
    CURSORINFO curr_cursor_info_;
    CURSORINFO prev_cursor_info_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMirror);
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_MIRROR_H
