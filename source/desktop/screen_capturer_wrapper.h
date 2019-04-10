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

#ifndef DESKTOP__SCREEN_CAPTURER_WRAPPER_H
#define DESKTOP__SCREEN_CAPTURER_WRAPPER_H

#include "base/macros_magic.h"
#include "base/thread_checker.h"
#include "base/win/scoped_thread_desktop.h"
#include "desktop/screen_capturer.h"

#include <cstdint>
#include <memory>

namespace desktop {

class EffectsDisabler;
class WallpaperDisabler;

class ScreenCapturerWrapper
{
public:
    ScreenCapturerWrapper(uint32_t flags);
    ~ScreenCapturerWrapper();

    enum Flags
    {
        DISABLE_EFFECTS = 1,
        DISABLE_WALLPAPER = 2
    };

    int screenCount();
    bool screenList(ScreenCapturer::ScreenList* screens);
    bool selectScreen(ScreenCapturer::ScreenId screen_id);
    const Frame* captureFrame();

private:
    bool switchToInputDesktop();
    void atDesktopSwitch();

    const uint32_t flags_;

    base::ScopedThreadDesktop desktop_;

    std::unique_ptr<ScreenCapturer> capturer_;

    std::unique_ptr<EffectsDisabler> effects_disabler_;
    std::unique_ptr<WallpaperDisabler> wallpaper_disabler_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWrapper);
};

} // namespace desktop

#endif // DESKTOP__SCREEN_CAPTURER_WRAPPER_H
