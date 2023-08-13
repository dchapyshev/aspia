//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/desktop/desktop_environment.h"
#include "base/desktop/desktop_resizer.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/power_save_blocker.h"
#include "base/ipc/shared_memory_factory.h"

#if defined(OS_WIN)
#include "base/desktop/screen_capturer_dxgi.h"
#include "base/desktop/screen_capturer_gdi.h"
#include "base/desktop/screen_capturer_mirror.h"
#include "base/win/windows_version.h"
#elif defined(OS_LINUX)
// TODO
#elif defined(OS_MAC)
// TODO
#else
#error Platform support not implemented
#endif

namespace base {

//--------------------------------------------------------------------------------------------------
ScreenCapturerWrapper::ScreenCapturerWrapper(ScreenCapturer::Type preferred_type,
                                             Delegate* delegate)
    : preferred_type_(preferred_type),
      delegate_(delegate),
      power_save_blocker_(std::make_unique<PowerSaveBlocker>()),
      environment_(DesktopEnvironment::create())
{
    LOG(LS_INFO) << "Ctor";

    switchToInputDesktop();

#if defined(OS_WIN)
    // If the monitor is turned off, this call will turn it on.
    if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED))
    {
        PLOG(LS_WARNING) << "SetThreadExecutionState failed";
    }

    wchar_t desktop[100] = { 0 };
    if (desktop_.assignedDesktop().name(desktop, sizeof(desktop)))
    {
        if (_wcsicmp(desktop, L"Screen-saver") == 0)
        {
            auto send_key = [](WORD key_code, DWORD flags)
            {
                INPUT input;
                memset(&input, 0, sizeof(input));

                input.type       = INPUT_KEYBOARD;
                input.ki.wVk     = key_code;
                input.ki.dwFlags = flags;
                input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));

                // Do the keyboard event.
                if (!SendInput(1, &input, sizeof(input)))
                {
                    PLOG(LS_WARNING) << "SendInput failed";
                }
            };

            send_key(VK_SPACE, 0);
            send_key(VK_SPACE, KEYEVENTF_KEYUP);
        }
    }
    else
    {
        LOG(LS_WARNING) << "Unable to get name of desktop";
    }
#endif // defined(OS_WIN)

    selectCapturer();
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerWrapper::~ScreenCapturerWrapper()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::selectScreen(ScreenCapturer::ScreenId screen_id, const Size& resolution)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (screen_id == screen_capturer_->currentScreen())
    {
        if (resolution.isEmpty())
        {
            LOG(LS_WARNING) << "Empty resolution";
        }
        else
        {
            if (!resizer_)
            {
                LOG(LS_WARNING) << "No desktop resizer";
            }
            else
            {
                LOG(LS_INFO) << "Change resolution for screen " << screen_id << " to: " << resolution;
                if (!resizer_->setResolution(screen_id, resolution))
                {
                    LOG(LS_WARNING) << "setResolution failed";
                    return;
                }
            }
        }
    }
    else
    {
        LOG(LS_INFO) << "Try to select screen: " << screen_id;

        if (!screen_capturer_->selectScreen(screen_id))
        {
            LOG(LS_ERROR) << "ScreenCapturer::selectScreen failed";
        }
        else
        {
            LOG(LS_INFO) << "Screen " << screen_id << " selected";
            last_screen_id_ = screen_id;
        }
    }

    ScreenCapturer::ScreenList screen_list;
    if (screen_capturer_->screenList(&screen_list))
    {
        LOG(LS_INFO) << "Received an updated list of screens";

        if (resizer_)
        {
            screen_list.resolutions = resizer_->supportedResolutions(screen_id);
            if (screen_list.resolutions.empty())
            {
                LOG(LS_INFO) << "No supported resolutions";
            }

            for (const auto& resolition : screen_list.resolutions)
            {
                LOG(LS_INFO) << "Supported resolution: " << resolition;
            }
        }
        else
        {
            LOG(LS_INFO) << "No desktop resizer";
        }

        for (const auto& screen : screen_list.screens)
        {
            LOG(LS_INFO) << "Screen #" << screen.id << " (position: " << screen.position
                         << " resolution: " << screen.resolution << " DPI: " << screen.dpi << ")";
        }

        delegate_->onScreenListChanged(screen_list, screen_id);
    }
    else
    {
        LOG(LS_ERROR) << "ScreenCapturer::screenList failed";
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::captureFrame()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer NOT initialized";
        return;
    }

    switchToInputDesktop();

    ++capture_counter_;

    int count = screen_capturer_->screenCount();
    if (screen_count_ != count)
    {
        LOG(LS_INFO) << "Screen count changed: " << count << " (old: " << screen_count_ << ")";

        resizer_.reset();
        resizer_ = DesktopResizer::create();

        screen_count_ = count;
        selectScreen(defaultScreen(), Size());
    }

    ScreenCapturer::Error error;
    const Frame* frame = screen_capturer_->captureFrame(&error);
    if (!frame)
    {
        switch (error)
        {
            case ScreenCapturer::Error::TEMPORARY:
                delegate_->onScreenCaptureError(error);
                return;

            case ScreenCapturer::Error::PERMANENT:
            {
                ++permanent_error_count_;

                LOG(LS_WARNING) << "Permanent error detected (" << permanent_error_count_ << ")";
                selectCapturer();

                delegate_->onScreenCaptureError(error);
            }
            return;

            default:
                NOTREACHED();
                break;
        }
    }
    else
    {
        permanent_error_count_ = 0;
    }

    const MouseCursor* mouse_cursor = nullptr;

    // Cursor capture only on even frames.
    if ((capture_counter_ % 2) == 0)
    {
        mouse_cursor = screen_capturer_->captureCursor();

        if (enable_cursor_position_)
        {
            Point cursor_pos = screen_capturer_->cursorPosition();

            int32_t delta_x = std::abs(cursor_pos.x() - last_cursor_pos_.x());
            int32_t delta_y = std::abs(cursor_pos.y() - last_cursor_pos_.y());

            if (delta_x > 1 || delta_y > 1)
            {
                delegate_->onCursorPositionChanged(cursor_pos);
                last_cursor_pos_ = cursor_pos;
            }
        }
    }

    delegate_->onScreenCaptured(frame, mouse_cursor);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::setSharedMemoryFactory(SharedMemoryFactory* shared_memory_factory)
{
    shared_memory_factory_ = shared_memory_factory;

    if (screen_capturer_)
        screen_capturer_->setSharedMemoryFactory(shared_memory_factory);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableWallpaper(bool enable)
{
    if (environment_)
    {
        environment_->setWallpaper(enable);
    }
    else
    {
        LOG(LS_WARNING) << "Desktop environment not initialized";
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableEffects(bool enable)
{
    if (environment_)
    {
        environment_->setEffects(enable);
    }
    else
    {
        LOG(LS_WARNING) << "Desktop environment not initialized";
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableFontSmoothing(bool enable)
{
    if (environment_)
    {
        environment_->setFontSmoothing(enable);
    }
    else
    {
        LOG(LS_WARNING) << "Desktop environment not initialized";
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableCursorPosition(bool enable)
{
    enable_cursor_position_ = enable;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerWrapper::defaultScreen()
{
    ScreenCapturer::ScreenList screen_list;
    if (screen_capturer_->screenList(&screen_list))
    {
        for (const auto& screen : screen_list.screens)
        {
            if (screen.is_primary)
            {
                LOG(LS_INFO) << "Primary screen found: " << screen.id;
                return screen.id;
            }
        }
    }
    else
    {
        LOG(LS_WARNING) << "ScreenCapturer::screenList failed";
    }

    LOG(LS_INFO) << "Primary screen NOT found";
    return ScreenCapturer::kFullDesktopScreenId;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::selectCapturer()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    LOG(LS_INFO) << "Selecting screen capturer. Preferred capturer: "
                 << ScreenCapturer::typeToString(preferred_type_);

#if defined(OS_WIN)
    auto try_mirror_capturer = [this]()
    {
        // Mirror screen capture is available only in Windows 7/2008 R2.
        if (win::windowsVersion() == base::win::VERSION_WIN7)
        {
            LOG(LS_INFO) << "Windows 7/2008R2 detected. Try to initialize MIRROR capturer";

            std::unique_ptr<ScreenCapturerMirror> capturer_mirror =
                std::make_unique<ScreenCapturerMirror>();

            if (capturer_mirror->isSupported())
            {
                LOG(LS_INFO) << "Using MIRROR capturer";
                screen_capturer_ = std::move(capturer_mirror);
            }
            else
            {
                LOG(LS_INFO) << "MIRROR capturer unavailable";
            }
        }
        else
        {
            LOG(LS_INFO) << "Windows version is not equal to 7/2008R2. MIRROR capturer unavailable";
        }
    };

    static const int kMaxPermanentErrorCount = 5;

    if (permanent_error_count_ >= kMaxPermanentErrorCount)
    {
        // Skip other capturer types and using GDI capturer.
        LOG(LS_INFO) << "Number of permanent errors has been exceeded. Reset to GDI capturer";
        permanent_error_count_ = 0;
    }
    else if (preferred_type_ == ScreenCapturer::Type::WIN_DXGI ||
             preferred_type_ == ScreenCapturer::Type::DEFAULT)
    {
        if (win::windowsVersion() >= win::VERSION_WIN8)
        {
            // Desktop Duplication API is available in Windows 8+.
            std::unique_ptr<ScreenCapturerDxgi> capturer_dxgi =
                std::make_unique<ScreenCapturerDxgi>();
            if (capturer_dxgi->isSupported())
            {
                LOG(LS_INFO) << "Using DXGI capturer";
                screen_capturer_ = std::move(capturer_dxgi);
            }
        }
        else
        {
            try_mirror_capturer();
        }
    }
    else if (preferred_type_ == ScreenCapturer::Type::WIN_MIRROR)
    {
        try_mirror_capturer();
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

    screen_capturer_->setSharedMemoryFactory(shared_memory_factory_);
    if (last_screen_id_ != ScreenCapturer::kInvalidScreenId)
    {
        LOG(LS_INFO) << "Restore selected screen: " << last_screen_id_;
        selectScreen(last_screen_id_, Size());
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::switchToInputDesktop()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(OS_WIN)
    // Switch to the desktop receiving user input if different from the current one.
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        wchar_t new_name[128] = { 0 };
        input_desktop.name(new_name, sizeof(new_name));

        wchar_t old_name[128] = { 0 };
        desktop_.assignedDesktop().name(old_name, sizeof(old_name));

        LOG(LS_INFO) << "Input desktop changed from '" << old_name << "' to '" << new_name << "'";

        if (screen_capturer_)
            screen_capturer_->reset();

        // If setThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.setThreadDesktop(std::move(input_desktop));

        if (environment_)
        {
            environment_->onDesktopChanged();
        }
        else
        {
            LOG(LS_WARNING) << "Desktop environment not initialized";
        }
    }
#endif // defined(OS_WIN)
}

} // namespace base
