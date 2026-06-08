//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/screen_capturer_gdi.h"

#include "base/logging.h"
#include "base/desktop/win/screen_capture_utils.h"
#include "base/desktop/frame_dib.h"
#include "base/desktop/differ.h"
#include "base/win/scoped_select_object.h"

#include <dwmapi.h>

//--------------------------------------------------------------------------------------------------
ScreenCapturerGdi::ScreenCapturerGdi(QObject* parent)
    : ScreenCapturerWin(Type::WIN_GDI, parent)
{
    LOG(INFO) << "Ctor";

    dwmapi_dll_ = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dwmapi_dll_)
    {
        dwm_enable_composition_func_ = reinterpret_cast<DwmEnableCompositionFunc>(
            GetProcAddress(dwmapi_dll_, "DwmEnableComposition"));
        if (!dwm_enable_composition_func_)
        {
            PLOG(ERROR) << "Unable to load DwmEnableComposition function";
        }

        dwm_is_composition_enabled_func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
            GetProcAddress(dwmapi_dll_, "DwmIsCompositionEnabled"));
        if (!dwm_is_composition_enabled_func_)
        {
            PLOG(ERROR) << "Unable to load DwmIsCompositionEnabled function";
        }
    }
    else
    {
        PLOG(ERROR) << "Unable to load dwmapi.dll";
    }
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerGdi::~ScreenCapturerGdi()
{
    LOG(INFO) << "Dtor";

    if (composition_changed_ && dwm_enable_composition_func_)
        dwm_enable_composition_func_(DWM_EC_ENABLECOMPOSITION);

    if (dwmapi_dll_)
        FreeLibrary(dwmapi_dll_);
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerGdi::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::selectScreen(ScreenId screen_id)
{
    LOG(INFO) << "Select screen with ID:" << screen_id;

    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
    {
        LOG(ERROR) << "Invalid screen";
        return false;
    }

    // At next screen capture, the resources are recreated.
    desktop_dc_rect_ = QRect();

    current_screen_id_ = screen_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerGdi::captureFrame(Error* error)
{
    DCHECK(error);

    queue_.moveToNextFrame();
    *error = Error::TEMPORARY;

    if (!prepareCaptureResources())
        return nullptr;

    screen_rect_ = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect_.isEmpty())
    {
        LOG(ERROR) << "Failed to get screen rect";
        *error = Error::PERMANENT;
        return nullptr;
    }

    if (!queue_.currentFrame() || queue_.currentFrame()->size() != screen_rect_.size())
    {
        DCHECK(desktop_dc_);
        DCHECK(memory_dc_);

        std::unique_ptr<Frame> frame = FrameDib::create(screen_rect_.size(), memory_dc_);
        if (!frame)
        {
            LOG(ERROR) << "Failed to create frame buffer";
            return nullptr;
        }

        frame->setCapturerType(static_cast<quint32>(type()));
        queue_.replaceCurrentFrame(std::move(frame));
    }

    Frame* current = queue_.currentFrame();
    Frame* previous = queue_.previousFrame();

    {
        ScopedSelectObject select_object(
            memory_dc_, static_cast<FrameDib*>(current)->bitmap());

        if (!BitBlt(memory_dc_,
                    0, 0,
                    screen_rect_.width(), screen_rect_.height(),
                    desktop_dc_,
                    screen_rect_.left(), screen_rect_.top(),
                    CAPTUREBLT | SRCCOPY))
        {
            static thread_local int count = 0;

            if (count == 0)
            {
                LOG(ERROR) << "BitBlt failed";
            }

            if (++count > 10)
                count = 0;

            return nullptr;
        }
    }

    current->setTopLeft(screen_rect_.topLeft() - desktop_dc_rect_.topLeft());

    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(screen_rect_.size());
        *current->updatedRegion() += QRect(QPoint(0, 0), screen_rect_.size());
    }
    else
    {
        differ_->calcDirtyRegion(previous->frameData(), current->frameData(), current->updatedRegion());
    }

    *error = Error::SUCCEEDED;
    return current;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerGdi::desktopRect() const
{
    return desktop_dc_rect_;
}

//--------------------------------------------------------------------------------------------------
const QRect& ScreenCapturerGdi::currentScreenRect() const
{
    return screen_rect_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerGdi::reset()
{
    ScreenCapturerWin::reset();
    desktop_dc_.close();
    memory_dc_.reset();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerGdi::prepareCaptureResources()
{
    QRect desktop_rect = ScreenCaptureUtils::fullScreenRect();

    // If the display bounds have changed then recreate GDI resources.
    if (desktop_rect != desktop_dc_rect_)
    {
        LOG(INFO) << "Desktop rect changed from" << desktop_dc_rect_ << "to" << desktop_rect;

        desktop_dc_.close();
        memory_dc_.reset();

        desktop_dc_rect_ = QRect();
    }

    if (!desktop_dc_)
    {
        DCHECK(!memory_dc_);

        if (dwm_enable_composition_func_ && dwm_is_composition_enabled_func_)
        {
            BOOL enabled;
            HRESULT hr = dwm_is_composition_enabled_func_(&enabled);
            if (SUCCEEDED(hr) && enabled)
            {
                // Vote to disable Aero composited desktop effects while capturing.
                // Windows will restore Aero automatically if the process exits.
                // This has no effect under Windows 8 or higher.
                dwm_enable_composition_func_(DWM_EC_DISABLECOMPOSITION);
                composition_changed_ = true;
            }
        }

        // Create GDI device contexts to capture from the desktop into memory.
        desktop_dc_.getDC(nullptr);
        memory_dc_.reset(CreateCompatibleDC(desktop_dc_));
        if (!memory_dc_)
        {
            LOG(ERROR) << "CreateCompatibleDC failed";
            return false;
        }

        desktop_dc_rect_ = desktop_rect;

        // Make sure the frame buffers will be reallocated.
        queue_.reset();
    }

    return true;
}
