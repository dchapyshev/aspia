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

#include "base/desktop/win/dfmirage_helper.h"

#include "base/logging.h"
#include "base/desktop/frame.h"

namespace base {

namespace {

static const int kBytesPerPixel = 4;
static const int kBitsPerPixel = 32;
static const int kExtendedDeviceModeSize = 3072;

struct DeviceMode : DEVMODEW
{
    char extension[kExtendedDeviceModeSize];
};

} // namespace

//--------------------------------------------------------------------------------------------------
DFMirageHelper::DFMirageHelper(const Rect& screen_rect)
    : screen_rect_(screen_rect)
{
    DCHECK(!screen_rect_.isEmpty());

    memset(&get_changes_buffer_, 0, sizeof(get_changes_buffer_));
}

//--------------------------------------------------------------------------------------------------
DFMirageHelper::~DFMirageHelper()
{
    if (is_mapped_)
        mapMemory(false);

    if (is_attached_)
        MirrorHelper::attachToDesktop(device_key_, false);

    if (is_updated_)
        update(false);
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<DFMirageHelper> DFMirageHelper::create(const Rect& screen_rect)
{
    std::unique_ptr<DFMirageHelper> helper(new DFMirageHelper(screen_rect));

    if (!MirrorHelper::findDisplayDevice("Mirage Driver",
                                         &helper->device_name_,
                                         &helper->device_key_))
    {
        LOG(LS_ERROR) << "Could not find dfmirage mirror driver";
        return nullptr;
    }

    if (!MirrorHelper::attachToDesktop(helper->device_key_, true))
    {
        LOG(LS_ERROR) << "Could not attach mirror driver to desktop";
        return nullptr;
    }

    helper->is_attached_ = true;

    if (!helper->update(true))
    {
        LOG(LS_ERROR) << "Could not load mirror driver";
        return nullptr;
    }

    helper->is_updated_ = true;

    if (!helper->mapMemory(true))
    {
        LOG(LS_ERROR) << "Could not map memory for mirror driver";
        return nullptr;
    }

    helper->is_mapped_ = true;

    LOG(LS_INFO) << "DFMirage helper created with rect: " << screen_rect;
    return helper;
}

//--------------------------------------------------------------------------------------------------
void DFMirageHelper::addUpdatedRects(Region* updated_region) const
{
    DCHECK(updated_region);

    Rect frame_rect = Rect::makeSize(screen_rect_.size());

    const int next_update = static_cast<int>(get_changes_buffer_.changes_buffer->counter);

    for (int i = last_update_; i != next_update; i = (i + 1) % kDfmMaxChanges)
    {
        const DfmRect* rect = &get_changes_buffer_.changes_buffer->records[i].rect;

        Rect updated_rect = Rect::makeLTRB(rect->left, rect->top, rect->right, rect->bottom);
        updated_rect.intersectWith(frame_rect);

        updated_region->addRect(updated_rect);
    }

    last_update_ = next_update;
}

//--------------------------------------------------------------------------------------------------
void DFMirageHelper::copyRegion(Frame* frame, const Region& updated_region) const
{
    DCHECK(frame);

    const quint8* source_buffer = get_changes_buffer_.user_buffer;
    const int source_stride = kBytesPerPixel * screen_rect_.width();

    for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
    {
        const Rect& rect = it.rect();
        const int source_offset =
            source_stride * rect.y() + static_cast<int>(sizeof(quint32)) * rect.x();
        frame->copyPixelsFrom(source_buffer + source_offset, source_stride, rect);
    }
}

//--------------------------------------------------------------------------------------------------
bool DFMirageHelper::update(bool load)
{
    static const DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;

    DeviceMode device_mode;
    device_mode.dmDriverExtra = 2 * sizeof(DWORD);

    DWORD* extended_header = reinterpret_cast<DWORD*>(&device_mode.extension[0]);
    extended_header[0] = dmf_devmodewext_magic_sig;
    extended_header[1] = 0;

    WORD extra_saved = device_mode.dmDriverExtra;

    memset(&device_mode, 0, sizeof(device_mode));

    device_mode.dmSize        = sizeof(DEVMODEW);
    device_mode.dmDriverExtra = extra_saved;
    device_mode.dmFields      = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

    if (load)
    {
        device_mode.dmPelsWidth  = static_cast<DWORD>(screen_rect_.width());
        device_mode.dmPelsHeight = static_cast<DWORD>(screen_rect_.height());
        device_mode.dmPosition.x = screen_rect_.x();
        device_mode.dmPosition.y = screen_rect_.y();
        device_mode.dmBitsPerPel = kBitsPerPixel;
    }

    wcsncpy_s(device_mode.dmDeviceName, std::size(device_mode.dmDeviceName),
              reinterpret_cast<const wchar_t*>(device_name_.utf16()), device_name_.length());

    LONG status = ChangeDisplaySettingsExW(
        reinterpret_cast<const wchar_t*>(device_name_.utf16()), &device_mode, nullptr,
        CDS_UPDATEREGISTRY, nullptr);
    if (status < 0)
    {
        LOG(LS_ERROR) << "ChangeDisplaySettingsExW failed: " << status;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool DFMirageHelper::mapMemory(bool map)
{
    if (map)
    {
        driver_dc_.reset(CreateDCW(reinterpret_cast<const wchar_t*>(device_name_.utf16()), nullptr,
                                   nullptr, nullptr));
        if (!driver_dc_.isValid())
        {
            PLOG(LS_ERROR) << "CreateDCW failed";
            return false;
        }

        int ret = ExtEscape(driver_dc_, DFM_ESC_USM_PIPE_MAP, 0, nullptr,
                            sizeof(get_changes_buffer_),
                            reinterpret_cast<LPSTR>(&get_changes_buffer_));
        if (ret <= 0)
        {
            LOG(LS_ERROR) << "ExtEscape failed: " << ret;
            return false;
        }
    }
    else
    {
        int ret = ExtEscape(driver_dc_, DFM_ESC_USM_PIPE_UNMAP,
                            sizeof(get_changes_buffer_),
                            reinterpret_cast<LPSTR>(&get_changes_buffer_),
                            0, nullptr);
        if (ret <= 0)
        {
            LOG(LS_ERROR) << "ExtEscape failed: " << ret;
        }

        memset(&get_changes_buffer_, 0, sizeof(get_changes_buffer_));
        driver_dc_.reset();
    }

    return true;
}

} // namespace base
