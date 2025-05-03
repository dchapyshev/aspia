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

#include "base/desktop/win/mv2_helper.h"

#include "base/logging.h"
#include "base/desktop/frame.h"

namespace base {

namespace {

static const int kBytesPerPixel = 4;
static const int kBitsPerPixel = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
Mv2Helper::Mv2Helper(const Rect& screen_rect)
    : screen_rect_(screen_rect)
{
    DCHECK(!screen_rect_.isEmpty());
}

//--------------------------------------------------------------------------------------------------
Mv2Helper::~Mv2Helper()
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
std::unique_ptr<Mv2Helper> Mv2Helper::create(const Rect& screen_rect)
{
    std::unique_ptr<Mv2Helper> helper(new Mv2Helper(screen_rect));

    if (!MirrorHelper::findDisplayDevice("mv video hook driver2",
                                         &helper->device_name_,
                                         &helper->device_key_))
    {
        LOG(LS_ERROR) << "Could not find mv2 mirror driver";
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

    LOG(LS_INFO) << "MV2 helper created with rect: " << screen_rect;
    return helper;
}

//--------------------------------------------------------------------------------------------------
void Mv2Helper::addUpdatedRects(Region* updated_region) const
{
    DCHECK(updated_region);

    Rect frame_rect = Rect::makeSize(screen_rect_.size());

    const int next_update = static_cast<int>(changes_buffer_->counter);

    for (int i = last_update_; i != next_update; i = (i + 1) % kMv2MaxChanges)
    {
        const Mv2Rect* rect = &changes_buffer_->records[i].rect;

        Rect updated_rect = Rect::makeLTRB(rect->left, rect->top, rect->right, rect->bottom);
        updated_rect.intersectWith(frame_rect);

        updated_region->addRect(updated_rect);
    }

    last_update_ = next_update;
}

//--------------------------------------------------------------------------------------------------
void Mv2Helper::copyRegion(Frame* frame, const Region& updated_region) const
{
    DCHECK(frame);

    const quint8* source_buffer = screen_buffer_;
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
bool Mv2Helper::update(bool load)
{
    DEVMODE device_mode;
    memset(&device_mode, 0, sizeof(device_mode));

    device_mode.dmSize = sizeof(DEVMODEW);
    device_mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

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
bool Mv2Helper::mapMemory(bool map)
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

        base::ScopedHandle file0;
        base::ScopedHandle file1;

        file0.reset(CreateFileW(L"c:\\video0.dat",
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr));
        file1.reset(CreateFileW(L"c:\\video1.dat",
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr));

        base::ScopedHandle file;

        if (file0.isValid() && !file1.isValid())
            file.reset(file0.release());

        if (file1.isValid() && !file0.isValid())
            file.reset(file1.release());

        if (file0.isValid() && file1.isValid())
        {
            const DWORD size0 = GetFileSize(file0, nullptr);
            const DWORD size1 = GetFileSize(file1, nullptr);

            const DWORD buffer_size = static_cast<DWORD>(kBytesPerPixel *
                                      screen_rect_.width() * screen_rect_.height() +
                                      static_cast<int>(sizeof(Mv2ChangesBuffer)));

            if (size0 == buffer_size)
                file.reset(file0.release());

            if (size1 == buffer_size)
                file.reset(file1.release());
        }

        if (!file.isValid())
        {
            LOG(LS_ERROR) << "Unable to open file";
            return false;
        }

        base::ScopedHandle file_map(
            CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr));
        if (!file_map.isValid())
        {
            PLOG(LS_ERROR) << "CreateFileMappingW failed";
            return false;
        }

        shared_buffer_ = reinterpret_cast<quint8*>(
            MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0));
        if (!shared_buffer_)
        {
            PLOG(LS_ERROR) << "MapViewOfFile failed";
            return false;
        }

        screen_buffer_ = shared_buffer_ + sizeof(Mv2ChangesBuffer);
        changes_buffer_ = reinterpret_cast<Mv2ChangesBuffer*>(shared_buffer_);
    }
    else
    {
        UnmapViewOfFile(shared_buffer_);

        shared_buffer_ = nullptr;

        screen_buffer_ = nullptr;
        changes_buffer_ = nullptr;

        driver_dc_.reset();
    }

    return true;
}

} // namespace base
