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

#include "base/desktop/screen_capturer_wrapper.h"

#include "base/logging.h"
#include "base/desktop/cursor_capturer.h"
#include "base/desktop/desktop_environment.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/power_save_blocker.h"
#include "base/ipc/shared_memory_factory.h"

#if defined(OS_WIN)
#include "base/desktop/cursor_capturer_win.h"
#include "base/desktop/screen_capturer_dxgi.h"
#include "base/desktop/screen_capturer_gdi.h"
#include "base/win/windows_version.h"
#elif defined(OS_LINUX)
#else
#endif

namespace base {

ScreenCapturerWrapper::ScreenCapturerWrapper(Delegate* delegate)
    : delegate_(delegate),
      power_save_blocker_(std::make_unique<PowerSaveBlocker>()),
      environment_(std::make_unique<DesktopEnvironment>())
{
#if defined(OS_WIN)
    // If the monitor is turned off, this call will turn it on.
    SetThreadExecutionState(ES_DISPLAY_REQUIRED);
#endif // defined(OS_WIN)

    switchToInputDesktop();
    selectCapturer();
}

ScreenCapturerWrapper::~ScreenCapturerWrapper() = default;

void ScreenCapturerWrapper::selectScreen(ScreenCapturer::ScreenId screen_id)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    LOG(LS_INFO) << "Try to select screen: " << screen_id;

    if (screen_capturer_->selectScreen(screen_id))
    {
        LOG(LS_INFO) << "Screen " << screen_id << " selected";

        ScreenCapturer::ScreenList screens;

        if (screen_capturer_->screenList(&screens))
        {
            LOG(LS_INFO) << "Received an updated list of screens";
            delegate_->onScreenListChanged(screens, screen_id);
        }
        else
        {
            LOG(LS_ERROR) << "ScreenCapturer::screenList failed";
        }
    }
    else
    {
        LOG(LS_ERROR) << "ScreenCapturer::selectScreen failed";
    }
}

void ScreenCapturerWrapper::captureFrame()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer NOT initialized";
        return;
    }

    switchToInputDesktop();

    int count = screen_capturer_->screenCount();
    if (screen_count_ != count)
    {
        LOG(LS_INFO) << "Screen count changed: " << count;

        screen_count_ = count;
        selectScreen(defaultScreen());
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

    delegate_->onScreenCaptured(frame, cursor_capturer_->captureCursor());
}

void ScreenCapturerWrapper::setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory)
{
    screen_capturer_->setSharedMemoryFactory(shared_memory_factory);
}

void ScreenCapturerWrapper::enableWallpaper(bool enable)
{
    environment_->setWallpaper(enable);
}

void ScreenCapturerWrapper::enableEffects(bool enable)
{
    environment_->setEffects(enable);
}

void ScreenCapturerWrapper::enableFontSmoothing(bool enable)
{
    environment_->setFontSmoothing(enable);
}

ScreenCapturer::ScreenId ScreenCapturerWrapper::defaultScreen()
{
    ScreenCapturer::ScreenList screens;
    if (screen_capturer_->screenList(&screens))
    {
        for (const auto& screen : screens)
        {
            if (screen.is_primary)
            {
                LOG(LS_INFO) << "Primary screen found: " << screen.id;
                return screen.id;
            }
        }
    }

    LOG(LS_INFO) << "Primary screen NOT found";
    return ScreenCapturer::kFullDesktopScreenId;
}

void ScreenCapturerWrapper::selectCapturer()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(OS_WIN)
    cursor_capturer_ = std::make_unique<CursorCapturerWin>();

    if (win::windowsVersion() >= win::VERSION_WIN8)
    {
        // Desktop Duplication API is available in Windows 8+.
        std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi = std::make_unique<ScreenCapturerDxgi>();
        if (capturer_dxgi->isSupported())
        {
            LOG(LS_INFO) << "Using DXGI capturer";
            screen_capturer_ = std::move(capturer_dxgi);
        }
    }

    if (!screen_capturer_)
    {
        LOG(LS_INFO) << "Using GDI capturer";
        screen_capturer_ = std::make_unique<ScreenCapturerGdi>();
    }
#elif defined(OS_LINUX)
    NOTIMPLEMENTED();
#elif defined(OS_MAC)
    NOTIMPLEMENTED();
#else
    NOTIMPLEMENTED();
#endif
}

void ScreenCapturerWrapper::switchToInputDesktop()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(OS_WIN)
    // Switch to the desktop receiving user input if different from the current one.
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        if (screen_capturer_)
            screen_capturer_->reset();

        if (cursor_capturer_)
            cursor_capturer_->reset();

        // If setThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.setThreadDesktop(std::move(input_desktop));
    }
#endif // defined(OS_WIN)
}

} // namespace base
