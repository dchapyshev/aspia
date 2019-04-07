//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__SCREEN_CAPTURER_DFMIRAGE_H
#define DESKTOP__SCREEN_CAPTURER_DFMIRAGE_H

#include "desktop/screen_capturer.h"

namespace desktop {

class DFMirageHelper;

class ScreenCapturerDFMirage : public ScreenCapturer
{
public:
    ScreenCapturerDFMirage();
    ~ScreenCapturerDFMirage();

    bool isSupported();

    // ScreenCapturer implementation.
    int screenCount() override;
    bool screenList(ScreenList* screens) override;
    bool selectScreen(ScreenId screen_id) override;
    const Frame* captureFrame() override;

protected:
    // ScreenCapturer implementation.
    void reset() override;

private:
    std::unique_ptr<DFMirageHelper> helper_;
    std::unique_ptr<Frame> frame_;

    int last_update_ = 0;
    int next_update_ = 0;

    ScreenId current_screen_id_ = kFullDesktopScreenId;
    QString current_device_key_;

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerDFMirage);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURER_DFMIRAGE_H
