//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#if defined(Q_OS_WINDOWS)
#include "base/desktop/screen_capturer_win.h"
#elif defined(Q_OS_LINUX)
#include "base/desktop/screen_capturer_x11.h"
#elif defined(Q_OS_MACOS)
// TODO
#else
#error Platform support not implemented
#endif

namespace base {

//--------------------------------------------------------------------------------------------------
ScreenCapturerWrapper::ScreenCapturerWrapper(ScreenCapturer::Type preferred_type, QObject* parent)
    : QObject(parent),
      preferred_type_(preferred_type),
      power_save_blocker_(std::make_unique<PowerSaveBlocker>()),
      environment_(DesktopEnvironment::create())
{
    LOG(LS_INFO) << "Ctor";
    selectCapturer(ScreenCapturer::Error::SUCCEEDED);
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerWrapper::~ScreenCapturerWrapper()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::selectScreen(ScreenCapturer::ScreenId screen_id, const Size& resolution)
{
    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer not initialized";
        return;
    }

    if (screen_id == screen_capturer_->currentScreen())
    {
        if (resolution.isEmpty())
        {
            LOG(LS_ERROR) << "Empty resolution";
        }
        else
        {
            if (!resizer_)
            {
                LOG(LS_ERROR) << "No desktop resizer";
            }
            else
            {
                LOG(LS_INFO) << "Change resolution for screen " << screen_id << " to: " << resolution;
                if (!resizer_->setResolution(screen_id, resolution))
                {
                    LOG(LS_ERROR) << "setResolution failed";
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

            for (const auto& resolition : std::as_const(screen_list.resolutions))
            {
                LOG(LS_INFO) << "Supported resolution: " << resolition;
            }
        }
        else
        {
            LOG(LS_INFO) << "No desktop resizer";
        }

        for (const auto& screen : std::as_const(screen_list.screens))
        {
            LOG(LS_INFO) << "Screen #" << screen.id << " (position: " << screen.position
                         << " resolution: " << screen.resolution << " DPI: " << screen.dpi << ")";
        }

        emit sig_screenListChanged(screen_list, screen_id);
    }
    else
    {
        LOG(LS_ERROR) << "ScreenCapturer::screenList failed";
    }
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::Error ScreenCapturerWrapper::captureFrame(
    const Frame** frame, const MouseCursor** mouse_cursor)
{
    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer NOT initialized";
        return ScreenCapturer::Error::TEMPORARY;
    }

    screen_capturer_->switchToInputDesktop();

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
    *frame = screen_capturer_->captureFrame(&error);
    if (!*frame)
    {
        switch (error)
        {
            case ScreenCapturer::Error::TEMPORARY:
                return error;

            case ScreenCapturer::Error::PERMANENT:
                selectCapturer(ScreenCapturer::Error::PERMANENT);
                return error;

            default:
                NOTREACHED();
                break;
        }
    }

    *mouse_cursor = nullptr;

    // Cursor capture only on even frames.
    if ((capture_counter_ % 2) == 0)
    {
        *mouse_cursor = screen_capturer_->captureCursor();

        if (enable_cursor_position_)
        {
            Point cursor_pos = screen_capturer_->cursorPosition();

            qint32 delta_x = std::abs(cursor_pos.x() - last_cursor_pos_.x());
            qint32 delta_y = std::abs(cursor_pos.y() - last_cursor_pos_.y());

            if (delta_x > 1 || delta_y > 1)
            {
                emit sig_cursorPositionChanged(cursor_pos);
                last_cursor_pos_ = cursor_pos;
            }
        }
    }

    return ScreenCapturer::Error::SUCCEEDED;
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
    if (!environment_)
    {
        LOG(LS_ERROR) << "Desktop environment not initialized";
        return;
    }

    environment_->setWallpaper(enable);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableEffects(bool enable)
{
    if (!environment_)
    {
        LOG(LS_ERROR) << "Desktop environment not initialized";
        return;
    }

    environment_->setEffects(enable);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableFontSmoothing(bool enable)
{
    if (!environment_)
    {
        LOG(LS_ERROR) << "Desktop environment not initialized";
        return;
    }

    environment_->setFontSmoothing(enable);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::enableCursorPosition(bool enable)
{
    enable_cursor_position_ = enable;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerWrapper::defaultScreen()
{
    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Screen capturer not initialized";
        return ScreenCapturer::kInvalidScreenId;
    }

    ScreenCapturer::ScreenList screen_list;
    if (screen_capturer_->screenList(&screen_list))
    {
        for (const auto& screen : std::as_const(screen_list.screens))
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
        LOG(LS_ERROR) << "ScreenCapturer::screenList failed";
    }

    LOG(LS_INFO) << "Primary screen NOT found";
    return ScreenCapturer::kFullDesktopScreenId;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerWrapper::selectCapturer(ScreenCapturer::Error last_error)
{
    LOG(LS_INFO) << "Selecting screen capturer. Preferred capturer: "
                 << ScreenCapturer::typeToString(preferred_type_);

    delete screen_capturer_;

#if defined(Q_OS_WINDOWS)
    screen_capturer_ = ScreenCapturerWin::create(preferred_type_, last_error, this);
#elif defined(Q_OS_LINUX)
    screen_capturer_ = ScreenCapturerX11::create();
    if (!screen_capturer_)
    {
        LOG(LS_ERROR) << "Unable to create X11 screen capturer";
        return;
    }
#elif defined(Q_OS_MACOS)
    NOTIMPLEMENTED();
#else
    NOTIMPLEMENTED();
#endif

    connect(screen_capturer_, &ScreenCapturer::sig_screenTypeChanged,
            this, &ScreenCapturerWrapper::sig_screenTypeChanged);

    connect(screen_capturer_, &ScreenCapturer::sig_desktopChanged, this, [this]()
    {
        if (!environment_)
        {
            LOG(LS_ERROR) << "Desktop environment not initialized";
            return;
        }

        environment_->onDesktopChanged();
    });

    screen_capturer_->setSharedMemoryFactory(shared_memory_factory_);
    if (last_screen_id_ != ScreenCapturer::kInvalidScreenId)
    {
        LOG(LS_INFO) << "Restore selected screen: " << last_screen_id_;
        selectScreen(last_screen_id_, Size());
    }
}

} // namespace base
