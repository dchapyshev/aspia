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

#include "base/desktop/frame_dib.h"

#include "base/logging.h"
#include "base/desktop/win/bitmap_info.h"
#include "base/ipc/shared_memory.h"
#include "base/ipc/shared_memory_factory.h"

namespace base {

FrameDib::FrameDib(const Size& size,
                   int stride,
                   uint8_t* data,
                   std::unique_ptr<SharedMemory> shared_memory,
                   HBITMAP bitmap)
    : Frame(size, stride, data, shared_memory.get()),
      bitmap_(bitmap),
      owned_shared_memory_(std::move(shared_memory))
{
    // Nothing
}

// static
std::unique_ptr<FrameDib> FrameDib::create(const Size& size,
                                           SharedMemoryFactory* shared_memory_factory,
                                           HDC hdc)
{
    const int bytes_per_row = size.width() * kBytesPerPixel;
    const size_t buffer_size = calcMemorySize(size, kBytesPerPixel);

    BitmapInfo bmi;
    memset(&bmi, 0, sizeof(bmi));

    bmi.header.biSize      = sizeof(bmi.header);
    bmi.header.biBitCount  = kBitsPerPixel;
    bmi.header.biSizeImage = buffer_size;
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = size.width();
    bmi.header.biHeight    = -size.height();
    bmi.header.biCompression = BI_BITFIELDS;
    bmi.u.mask.red   = 255 << 16;
    bmi.u.mask.green = 255 << 8;
    bmi.u.mask.blue  = 255 << 0;

    std::unique_ptr<SharedMemory> shared_memory;
    HANDLE section_handle = nullptr;

    if (shared_memory_factory)
    {
        shared_memory = shared_memory_factory->create(buffer_size);
        section_handle = shared_memory->handle();
    }

    void* data = nullptr;

    HBITMAP bitmap = CreateDIBSection(hdc,
                                      reinterpret_cast<LPBITMAPINFO>(&bmi),
                                      DIB_RGB_COLORS,
                                      &data,
                                      section_handle,
                                      0);
    if (!bitmap)
    {
        LOG(LS_WARNING) << "CreateDIBSection failed";
        return nullptr;
    }

    return std::unique_ptr<FrameDib>(new FrameDib(
        size, bytes_per_row, reinterpret_cast<uint8_t*>(data),
        std::move(shared_memory), bitmap));
}

} // namespace base
