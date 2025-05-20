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

#include "base/desktop/screen_capturer_mirror.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/win/cursor.h"
#include "base/desktop/win/dfmirage_helper.h"
#include "base/desktop/win/mv2_helper.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/shared_frame.h"
#include "base/desktop/shared_memory_frame.h"
#include "base/desktop/win/screen_capture_utils.h"
#include "base/ipc/shared_memory_factory.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
std::unique_ptr<MirrorHelper> createHelper(const Rect& screen_rect)
{
    std::unique_ptr<MirrorHelper> helper = Mv2Helper::create(screen_rect);
    if (!helper)
        helper = DFMirageHelper::create(screen_rect);

    return helper;
}

//--------------------------------------------------------------------------------------------------
bool isSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerMirror::ScreenCapturerMirror(QObject* parent)
    : ScreenCapturerWin(Type::WIN_MIRROR, parent)
{
    LOG(LS_INFO) << "Ctor";

    memset(&curr_cursor_info_, 0, sizeof(curr_cursor_info_));
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerMirror::~ScreenCapturerMirror()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMirror::isSupported()
{
    DWORD session_id;

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
        return false;
    }

    if (session_id != WTSGetActiveConsoleSessionId())
    {
        LOG(LS_INFO) << "Mirror driver not supported for RDP sessions";
        return false;
    }

    if (!helper_)
        helper_ = createHelper(ScreenCaptureUtils::fullScreenRect());

    return helper_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerMirror::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMirror::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerMirror::selectScreen(ScreenId screen_id)
{
    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
    {
        LOG(LS_ERROR) << "Invalid screen id passed: " << screen_id;
        return false;
    }

    reset();

    current_screen_id_ = screen_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerMirror::currentScreen() const
{
    return current_screen_id_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerMirror::captureFrame(Error* error)
{
    DCHECK(error);

    *error = prepareCaptureResources();
    if (*error != Error::SUCCEEDED)
        return nullptr;

    Region* updated_region = frame_->updatedRegion();

    // Clearing the region from previous changes.
    updated_region->clear();

    // Update the list of changed areas.
    helper_->addUpdatedRects(updated_region);

    // Exclude a region that should not be captured.
    updated_region->subtract(exclude_region_);

    // Copy the image of the modified areas into the frame.
    helper_->copyRegion(frame_.get(), *updated_region);

    const Rect& screen_rect = helper_->screenRect();

    frame_->setTopLeft(screen_rect.topLeft().subtract(desktop_rect_.topLeft()));
    return frame_.get();
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerMirror::captureCursor()
{
    if (!desktop_dc_.isValid())
    {
        desktop_dc_.getDC(nullptr);
        if (!desktop_dc_.isValid())
        {
            LOG(LS_ERROR) << "Unable to get desktop DC";
            return nullptr;
        }
    }

    memset(&curr_cursor_info_, 0, sizeof(curr_cursor_info_));

    // Note: cursor_info.hCursor does not need to be freed.
    curr_cursor_info_.cbSize = sizeof(curr_cursor_info_);
    if (GetCursorInfo(&curr_cursor_info_))
    {
        if (!isSameCursorShape(curr_cursor_info_, prev_cursor_info_))
        {
            if (curr_cursor_info_.flags == 0)
            {
                LOG(LS_INFO) << "No hardware cursor attached. Using default mouse cursor";

                // Host machine does not have a hardware mouse attached, we will send a default one
                // instead. Note, Windows automatically caches cursor resource, so we do not need
                // to cache the result of LoadCursor.
                curr_cursor_info_.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                if (!curr_cursor_info_.hCursor)
                {
                    PLOG(LS_ERROR) << "LoadCursorW failed";
                    return nullptr;
                }
            }

            mouse_cursor_.reset(mouseCursorFromHCursor(desktop_dc_, curr_cursor_info_.hCursor));
            if (mouse_cursor_)
            {
                prev_cursor_info_ = curr_cursor_info_;

                int dpi_x = GetDeviceCaps(desktop_dc_, LOGPIXELSX);
                int dpi_y = GetDeviceCaps(desktop_dc_, LOGPIXELSY);

                mouse_cursor_->dpi() = Point(dpi_x, dpi_y);
                return mouse_cursor_.get();
            }
        }
    }
    else
    {
        PLOG(LS_ERROR) << "GetCursorInfo failed";
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
Point ScreenCapturerMirror::cursorPosition()
{
    Point cursor_pos(curr_cursor_info_.ptScreenPos.x, curr_cursor_info_.ptScreenPos.y);
    cursor_pos = cursor_pos.subtract(helper_->screenRect().topLeft());
    return cursor_pos;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMirror::reset()
{
    desktop_dc_.close();
    helper_.reset();
    frame_.reset();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerMirror::updateExcludeRegion()
{
    exclude_region_.clear();

    if (current_screen_id_ != kFullDesktopScreenId)
        return;

    exclude_region_.addRect(desktop_rect_.moved(0, 0));

    ScreenList screen_list;
    if (!ScreenCaptureUtils::screenList(&screen_list))
    {
        LOG(LS_ERROR) << "Unable to get screen list";
        return;
    }

    for (const auto& screen : screen_list.screens)
    {
        std::wstring device_key;

        if (ScreenCaptureUtils::isScreenValid(screen.id, &device_key))
        {
            Rect screen_rect = ScreenCaptureUtils::screenRect(screen.id, device_key);

            int x = std::abs(screen_rect.x() - desktop_rect_.x());
            int y = std::abs(screen_rect.y() - desktop_rect_.y());
            int w = screen_rect.width();
            int h = screen_rect.height();

            exclude_region_.subtract(Rect::makeXYWH(x, y, w, h));
        }
    }
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerMirror::Error ScreenCapturerMirror::prepareCaptureResources()
{
    Rect desktop_rect = ScreenCaptureUtils::fullScreenRect();
    if (desktop_rect.isEmpty())
    {
        LOG(LS_ERROR) << "Failed to get desktop rect";
        return Error::TEMPORARY;
    }

    if (desktop_rect != desktop_rect_)
    {
        desktop_rect_ = desktop_rect;
        reset();
    }

    Rect screen_rect = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect.isEmpty())
    {
        LOG(LS_ERROR) << "Failed to get screen rect";
        return Error::PERMANENT;
    }

    if (helper_ && helper_->screenRect() != screen_rect)
        reset();

    if (frame_ && frame_->size() != screen_rect.size())
        reset();

    if (!helper_)
    {
        helper_ = createHelper(screen_rect);
        if (!helper_)
        {
            LOG(LS_ERROR) << "Failed to create mirror helper";
            return Error::PERMANENT;
        }

        updateExcludeRegion();
    }

    if (!frame_)
    {
        SharedMemoryFactory* factory = sharedMemoryFactory();
        if (factory)
        {
            frame_ = SharedMemoryFrame::create(screen_rect.size(), PixelFormat::ARGB(), factory);
        }
        else
        {
            frame_ = FrameAligned::create(screen_rect.size(), PixelFormat::ARGB(), 32);
        }

        if (!frame_)
        {
            LOG(LS_ERROR) << "Failed to create frame";
            return Error::PERMANENT;
        }

        frame_->setCapturerType(static_cast<quint32>(type()));
    }

    return Error::SUCCEEDED;
}

} // namespace base
