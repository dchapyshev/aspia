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

#include "base/desktop/screen_capturer_dxgi.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/win/screen_capture_utils.h"

namespace base {

namespace {

const int kMaxTemporaryErrorCount = 3;

//--------------------------------------------------------------------------------------------------
bool screenListFromDeviceNames(const QStringList& device_names,
                               ScreenCapturer::ScreenList* screen_list)
{
    DCHECK(screen_list->screens.empty());

    ScreenCapturer::ScreenList gdi_screens;
    if (!ScreenCaptureUtils::screenList(&gdi_screens))
    {
        LOG(LS_ERROR) << "screenList failed";
        return false;
    }

    ScreenCapturer::ScreenId max_screen_id = -1;

    for (const auto& screen : std::as_const(gdi_screens.screens))
        max_screen_id = std::max(max_screen_id, screen.id);

    LOG(LS_INFO) << "Device names count: " << device_names.size()
                 << ", GDI count: " << gdi_screens.screens.size()
                 << ", max screen id: " << max_screen_id;

    for (int device_index = 0; device_index < device_names.size(); ++device_index)
    {
        const QString& device_name = device_names[device_index];
        bool device_found = false;

        for (const auto& gdi_screen : std::as_const(gdi_screens.screens))
        {
            if (gdi_screen.title == device_name)
            {
                screen_list->screens.push_back(gdi_screen);
                device_found = true;
                break;
            }
        }

        if (!device_found)
        {
            LOG(LS_ERROR) << "Device '" << device_name << "' NOT found in list ("
                          << device_index << ")";

            // devices_names[i] has not been found in gdi_names, so use max_screen_id.
            ++max_screen_id;
            screen_list->screens.push_back(
                { max_screen_id, QString(), Point(), Size(), Point(), false });
        }
        else
        {
            LOG(LS_INFO) << "Device '" << device_name << "' found in list (" << device_index << ")";
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
int indexFromScreenId(ScreenCapturer::ScreenId id, const QStringList& device_names)
{
    ScreenCapturer::ScreenList screen_list;
    if (!screenListFromDeviceNames(device_names, &screen_list))
    {
        LOG(LS_ERROR) << "screenListFromDeviceNames failed";
        return -1;
    }

    DCHECK_EQ(device_names.size(), screen_list.screens.size());

    for (int i = 0; i < screen_list.screens.size(); ++i)
    {
        if (screen_list.screens[i].id == id)
        {
            LOG(LS_INFO) << "Screen with ID " << id << " found ("
                         << screen_list.screens[i].title << ")";
            return static_cast<int>(i);
        }
    }

    LOG(LS_ERROR) << "Screen with ID " << id << " NOT found";
    return -1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ScreenCapturerDxgi::ScreenCapturerDxgi(QObject* parent)
    : ScreenCapturerWin(Type::WIN_DXGI, parent),
      controller_(new DxgiDuplicatorController()),
      cursor_(std::make_unique<DxgiCursor>())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerDxgi::~ScreenCapturerDxgi()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerDxgi::isSupported()
{
    // Forwards isSupported() function call to DxgiDuplicatorController.
    return controller_->isSupported();
}

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCapturerDxgi::isCurrentSessionSupported()
{
    return DxgiDuplicatorController::isCurrentSessionSupported();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerDxgi::screenCount()
{
    return controller_->screenCount();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerDxgi::screenList(ScreenList* screens)
{
    QStringList device_names;

    if (!controller_->deviceNames(&device_names))
    {
        LOG(LS_ERROR) << "deviceNames failed";
        return false;
    }

    bool result = screenListFromDeviceNames(device_names, screens);

    dpi_for_rect_.clear();

    for (const auto& screen : std::as_const(screens->screens))
    {
        dpi_for_rect_.emplace_back(
            Rect::makeXYWH(screen.position, screen.resolution), screen.dpi);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerDxgi::selectScreen(ScreenId screen_id)
{
    LOG(LS_INFO) << "Select screen with ID: " << screen_id;

    if (screen_id == kFullDesktopScreenId)
    {
        current_screen_index_ = -1;
        current_screen_id_ = screen_id;
        return true;
    }

    QStringList device_names;
    if (!controller_->deviceNames(&device_names))
    {
        LOG(LS_ERROR) << "deviceNames failed";
        return false;
    }

    int index = indexFromScreenId(screen_id, device_names);
    if (index == -1)
    {
        LOG(LS_ERROR) << "indexFromScreenId failed";
        return false;
    }

    current_screen_index_ = index;
    current_screen_id_ = screen_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerDxgi::currentScreen() const
{
    return current_screen_id_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerDxgi::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();

    if (!queue_.currentFrame())
    {
        queue_.replaceCurrentFrame(
            std::make_unique<DxgiFrame>(controller_, sharedMemoryFactory()));
    }

    DxgiDuplicatorController::Result result;

    if (current_screen_index_ == -1)
    {
        result = controller_->duplicate(queue_.currentFrame(), cursor_.get());
    }
    else
    {
        result = controller_->duplicateMonitor(
            queue_.currentFrame(), cursor_.get(), current_screen_index_);
    }

    using DuplicateResult = DxgiDuplicatorController::Result;

    if (result != DuplicateResult::SUCCEEDED)
    {
        LOG(LS_ERROR) << "DxgiDuplicatorController failed to capture desktop, error code "
                      << DxgiDuplicatorController::resultName(result);
    }

    switch (result)
    {
        case DuplicateResult::SUCCEEDED:
        {
            temporary_error_count_ = 0;
            *error = Error::SUCCEEDED;
            return queue_.currentFrame()->frame().get();
        }

        case DuplicateResult::UNSUPPORTED_SESSION:
        {
            LOG(LS_ERROR) << "Current binary is running on a session not supported "
                             "by DirectX screen capturer";
            *error = Error::PERMANENT;
            return nullptr;
        }

        case DuplicateResult::FRAME_PREPARE_FAILED:
        {
            LOG(LS_ERROR) << "Failed to allocate a new Frame";
            // This usually means we do not have enough memory or SharedMemoryFactory cannot work
            // correctly.
            *error = Error::PERMANENT;
            return nullptr;
        }

        case DuplicateResult::INVALID_MONITOR_ID:
        {
            LOG(LS_ERROR) << "Invalid monitor id " << current_screen_index_;
            *error = Error::PERMANENT;
            return nullptr;
        }

        case DuplicateResult::INITIALIZATION_FAILED:
        case DuplicateResult::DUPLICATION_FAILED:
        default:
        {
            ++temporary_error_count_;

            if (temporary_error_count_ >= kMaxTemporaryErrorCount)
            {
                LOG(LS_ERROR) << "More than " << kMaxTemporaryErrorCount
                              << " temporary capture errors detected";
                *error = Error::PERMANENT;
            }
            else
            {
                *error = Error::TEMPORARY;
            }

            return nullptr;
        }
    }
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerDxgi::captureCursor()
{
    MouseCursor* mouse_cursor = cursor_->mouseCursor();
    if (mouse_cursor)
    {
        Point position = cursor_->nativePosition();

        for (const auto& item : dpi_for_rect_)
        {
            const Rect& rect = item.first;

            if (rect.contains(position))
            {
                const Point& dpi = item.second;
                mouse_cursor->dpi() = dpi;
            }
        }
    }

    return mouse_cursor;
}

//--------------------------------------------------------------------------------------------------
Point ScreenCapturerDxgi::cursorPosition()
{
    return cursor_->position();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerDxgi::reset()
{
    queue_.reset();
}

} // namespace base
