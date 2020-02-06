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

#include "desktop/screen_capturer_wrapper.h"

#include "base/logging.h"
#include "base/win/windows_version.h"
#include "desktop/cursor_capturer_win.h"
#include "desktop/mouse_cursor.h"
#include "desktop/screen_capturer_dxgi.h"
#include "desktop/screen_capturer_gdi.h"
#include "desktop/screen_capturer_mirror.h"
#include "desktop/win/effects_disabler.h"
#include "desktop/win/wallpaper_disabler.h"
#include "ipc/shared_memory_factory.h"

namespace desktop {

ScreenCapturerWrapper::ScreenCapturerWrapper(Delegate* delegate)
    : delegate_(delegate)
{
    switchToInputDesktop();
    atDesktopSwitch();

    // If the monitor is turned off, this call will turn it on.
    SetThreadExecutionState(ES_DISPLAY_REQUIRED);

    selectCapturer();
}

ScreenCapturerWrapper::~ScreenCapturerWrapper() = default;

void ScreenCapturerWrapper::selectScreen(ScreenCapturer::ScreenId screen_id)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (screen_capturer_->selectScreen(screen_id))
    {
        ScreenCapturer::ScreenList screens;

        if (screen_capturer_->screenList(&screens))
            delegate_->onScreenListChanged(screens, screen_id);
    }
}

void ScreenCapturerWrapper::captureFrame()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (switchToInputDesktop())
        atDesktopSwitch();

    int count = screen_capturer_->screenCount();
    if (screen_count_ != count)
    {
        screen_count_ = count;
        selectScreen(ScreenCapturer::kFullDesktopScreenId);
    }

    ScreenCapturer::Error error;
    const Frame* frame = screen_capturer_->captureFrame(&error);
    if (!frame)
    {
        switch (error)
        {
            case ScreenCapturer::Error::TEMPORARY:
                break;

            case ScreenCapturer::Error::PERMANENT:
                selectCapturer();
                break;

            default:
                NOTREACHED();
                break;
        }
    }

    std::unique_ptr<MouseCursor> mouse_cursor;
    mouse_cursor.reset(cursor_capturer_->captureCursor());

    delegate_->onScreenCaptured(frame, mouse_cursor.get());
}

void ScreenCapturerWrapper::setSharedMemoryFactory(ipc::SharedMemoryFactory* shared_memory_factory)
{
    screen_capturer_->setSharedMemoryFactory(shared_memory_factory);
}

void ScreenCapturerWrapper::enableWallpaper(bool enable)
{
    enable_wallpaper_ = enable;

    wallpaper_disabler_.reset();

    if (!enable)
        wallpaper_disabler_ = std::make_unique<WallpaperDisabler>();
}

void ScreenCapturerWrapper::enableEffects(bool enable)
{
    enable_effects_ = enable;

    effects_disabler_.reset();

    if (!enable)
        effects_disabler_ = std::make_unique<EffectsDisabler>();

}

void ScreenCapturerWrapper::selectCapturer()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    cursor_capturer_ = std::make_unique<CursorCapturerWin>();

    // Mirror screen capture is available only in Windows 7/2008 R2.
    if (base::win::windowsVersion() == base::win::VERSION_WIN7)
    {
        std::unique_ptr<ScreenCapturerMirror> capturer_mirror =
            std::make_unique<ScreenCapturerMirror>();

        if (capturer_mirror->isSupported())
        {
            LOG(LS_INFO) << "Using mirror capturer";
            screen_capturer_ = std::move(capturer_mirror);
            return;
        }
    }

    // Desktop Duplication API is available in Windows 8+.
    std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi = std::make_unique<ScreenCapturerDxgi>();
    if (capturer_dxgi->isSupported())
    {
        LOG(LS_INFO) << "Using DXGI capturer";
        screen_capturer_ = std::move(capturer_dxgi);
    }
    else
    {
        LOG(LS_INFO) << "Using GDI capturer";
        screen_capturer_ = std::make_unique<ScreenCapturerGdi>();
    }
}

bool ScreenCapturerWrapper::switchToInputDesktop()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Switch to the desktop receiving user input if different from the current one.
    base::Desktop input_desktop(base::Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        if (screen_capturer_)
            screen_capturer_->reset();

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

    if (!enable_effects_)
        effects_disabler_ = std::make_unique<EffectsDisabler>();

    if (!enable_wallpaper_)
        wallpaper_disabler_ = std::make_unique<WallpaperDisabler>();
}

} // namespace desktop
