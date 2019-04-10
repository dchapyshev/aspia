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

#include "desktop/screen_capturer_wrapper.h"

#include "base/logging.h"
#include "base/win/windows_version.h"
#include "desktop/screen_capturer_dfmirage.h"
#include "desktop/screen_capturer_dxgi.h"
#include "desktop/screen_capturer_gdi.h"
#include "desktop/win/effects_disabler.h"
#include "desktop/win/wallpaper_disabler.h"

namespace desktop {

ScreenCapturerWrapper::ScreenCapturerWrapper(uint32_t flags)
    : flags_(flags)
{
    switchToInputDesktop();
    atDesktopSwitch();

    // If the monitor is turned off, this call will turn it on.
    SetThreadExecutionState(ES_DISPLAY_REQUIRED);

    // DFMirage screen capture is available only in Windows 7/2008 R2.
    if (base::win::windowsVersion() == base::win::VERSION_WIN7)
    {
        std::unique_ptr<ScreenCapturerDFMirage> capturer_dfmirage =
            std::make_unique<ScreenCapturerDFMirage>();

        if (capturer_dfmirage->isSupported())
        {
            LOG(LS_INFO) << "Using DFMirage capturer";
            capturer_ = std::move(capturer_dfmirage);
            return;
        }
    }

    // Desktop Duplication API is available in Windows 8+.
    std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi = std::make_unique<ScreenCapturerDxgi>();
    if (capturer_dxgi->isSupported())
    {
        LOG(LS_INFO) << "Using DXGI capturer";
        capturer_ = std::move(capturer_dxgi);
    }
    else
    {
        LOG(LS_INFO) << "Using GDI capturer";
        capturer_ = std::make_unique<ScreenCapturerGdi>();
    }
}

ScreenCapturerWrapper::~ScreenCapturerWrapper() = default;

int ScreenCapturerWrapper::screenCount()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return capturer_->screenCount();
}

bool ScreenCapturerWrapper::screenList(ScreenCapturer::ScreenList* screens)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return capturer_->screenList(screens);
}

bool ScreenCapturerWrapper::selectScreen(ScreenCapturer::ScreenId screen_id)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return capturer_->selectScreen(screen_id);
}

const Frame* ScreenCapturerWrapper::captureFrame()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (switchToInputDesktop())
        atDesktopSwitch();

    return capturer_->captureFrame();
}

bool ScreenCapturerWrapper::switchToInputDesktop()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Switch to the desktop receiving user input if different from the current one.
    base::Desktop input_desktop(base::Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        if (capturer_)
            capturer_->reset();

        effects_disabler_.reset();
        wallpaper_disabler_.reset();

        // If setThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.setThreadDesktop(std::move(input_desktop));
        return true;
    }

    return false;
}

void ScreenCapturerWrapper::atDesktopSwitch()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (flags_ & DISABLE_EFFECTS)
        effects_disabler_ = std::make_unique<EffectsDisabler>();

    if (flags_ & DISABLE_WALLPAPER)
        wallpaper_disabler_ = std::make_unique<WallpaperDisabler>();
}

} // namespace desktop
