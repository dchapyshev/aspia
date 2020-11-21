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

#include "base/desktop/screen_capturer_dxgi.h"

#include "base/logging.h"
#include "base/desktop/win/screen_capture_utils.h"
#include "base/ipc/shared_memory_factory.h"
#include "base/strings/unicode.h"

namespace base {

namespace {

bool screenListFromDeviceNames(const std::vector<std::wstring>& device_names,
                               ScreenCapturer::ScreenList* screens)
{
    DCHECK(screens->empty());

    ScreenCapturer::ScreenList gdi_screens;

    if (!ScreenCaptureUtils::screenList(&gdi_screens))
        return false;

    ScreenCapturer::ScreenId max_screen_id = -1;

    for (const auto& screen : gdi_screens)
        max_screen_id = std::max(max_screen_id, screen.id);

    for (const auto& device_name : device_names)
    {
        bool device_found = false;

        for (const auto& gdi_screen : gdi_screens)
        {
            std::string device_name_utf8 = utf8FromWide(device_name);

            if (gdi_screen.title == device_name_utf8)
            {
                screens->push_back(gdi_screen);
                device_found = true;

                break;
            }
        }

        if (!device_found)
        {
            // devices_names[i] has not been found in gdi_names, so use max_screen_id.
            ++max_screen_id;
            screens->push_back({ max_screen_id, std::string() });
        }
    }

    return true;
}

int indexFromScreenId(ScreenCapturer::ScreenId id, const std::vector<std::wstring>& device_names)
{
    ScreenCapturer::ScreenList screens;

    if (!screenListFromDeviceNames(device_names, &screens))
        return -1;

    DCHECK_EQ(device_names.size(), screens.size());

    for (int i = 0; i < static_cast<int>(screens.size()); ++i)
    {
        if (screens[i].id == id)
            return i;
    }

    return -1;
}

} // namespace

ScreenCapturerDxgi::ScreenCapturerDxgi()
    : ScreenCapturer(Type::WIN_DXGI),
      controller_(std::make_shared<DxgiDuplicatorController>())
{
    // Nothing
}

ScreenCapturerDxgi::~ScreenCapturerDxgi() = default;

bool ScreenCapturerDxgi::isSupported()
{
    // Forwards isSupported() function call to DxgiDuplicatorController.
    return controller_->isSupported();
}

// static
bool ScreenCapturerDxgi::isCurrentSessionSupported()
{
    return DxgiDuplicatorController::isCurrentSessionSupported();
}

int ScreenCapturerDxgi::screenCount()
{
    return controller_->screenCount();
}

bool ScreenCapturerDxgi::screenList(ScreenList* screens)
{
    std::vector<std::wstring> device_names;

    if (!controller_->deviceNames(&device_names))
        return false;

    return screenListFromDeviceNames(device_names, screens);
}

bool ScreenCapturerDxgi::selectScreen(ScreenId screen_id)
{
    if (screen_id == kFullDesktopScreenId)
    {
        current_screen_id_ = screen_id;
        return true;
    }

    std::vector<std::wstring> device_names;

    if (!controller_->deviceNames(&device_names))
        return false;

    int index = indexFromScreenId(screen_id, device_names);
    if (index == -1)
        return false;

    current_screen_id_ = index;
    return true;
}

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

    if (current_screen_id_ == kFullDesktopScreenId)
        result = controller_->duplicate(queue_.currentFrame());
    else
        result = controller_->duplicateMonitor(queue_.currentFrame(), current_screen_id_);

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
            *error = Error::SUCCEEDED;
            return queue_.currentFrame()->frame();
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
            LOG(LS_ERROR) << "Invalid monitor id " << current_screen_id_;
            *error = Error::PERMANENT;
            return nullptr;
        }

        case DuplicateResult::INITIALIZATION_FAILED:
        case DuplicateResult::DUPLICATION_FAILED:
        default:
        {
            *error = Error::TEMPORARY;
            return nullptr;
        }
    }
}

void ScreenCapturerDxgi::reset()
{
    queue_.reset();
}

} // namespace base
