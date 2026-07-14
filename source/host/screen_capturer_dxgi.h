//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_SCREEN_CAPTURER_DXGI_H
#define HOST_SCREEN_CAPTURER_DXGI_H

#include "host/screen_capturer_win.h"
#include "host/win/dxgi_duplicator_controller.h"
#include "host/win/dxgi_frame.h"

#include <map>
#include <memory>

class ScreenCapturerDxgi final : public ScreenCapturerWin
{
    Q_OBJECT

public:
    explicit ScreenCapturerDxgi(QObject* parent = nullptr);
    ~ScreenCapturerDxgi() final;

    // Whether the system supports DXGI based capturing.
    bool isSupported();

    // Whether current process is running in a Windows session which is supported by
    // ScreenCapturerDxgi.
    // Usually using ScreenCapturerDxgi in unsupported sessions will fail.
    // But this behavior may vary on different Windows version. So consumers can
    // always try isSupported() function.
    static bool isCurrentSessionSupported();

    // ScreenCapturer implementation.
    int screenCount() final;
    bool screenList(ScreenList* screens) final;
    bool selectScreen(ScreenId screen_id) final;
    const Frame* captureFrame(Error* error) final;
    const QRect& desktopRect() const final;
    const QRect& currentScreenRect() const final;

protected:
    // ScreenCapturer implementation.
    void reset() final;

private:
    std::shared_ptr<DxgiDuplicatorController> controller_;

    int current_screen_index_ = 0;

    // One retained frame per monitor, keyed by the stable screen id (not the index, which shifts when
    // a monitor is (un)plugged). Keeping each monitor's last frame instead of dropping it on every
    // screen switch means switching back to a monitor reuses its content directly, so a static screen
    // is not shown black and the duplicator does not have to reconstruct a full frame.
    std::map<ScreenId, std::unique_ptr<DxgiFrame>> frames_;

    // Set on a screen switch, consumed by the next captured frame. The retained frame already holds
    // the monitor pixels, but the duplicator reports only what changed since the last capture (nothing
    // for a static screen). The whole frame must be marked updated so the downstream scaler/encoder
    // refreshes from these pixels for the key frame forced on the switch.
    bool force_full_update_ = false;

    int temporary_error_count_ = 0;

    Q_DISABLE_COPY_MOVE(ScreenCapturerDxgi)
};

#endif // HOST_SCREEN_CAPTURER_DXGI_H
