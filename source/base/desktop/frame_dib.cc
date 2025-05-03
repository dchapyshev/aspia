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

#include "base/desktop/frame_dib.h"

#include "base/logging.h"
#include "base/desktop/win/bitmap_info.h"
#include "base/ipc/shared_memory_factory.h"

namespace base {

//--------------------------------------------------------------------------------------------------
FrameDib::FrameDib(const Size& size,
                   const PixelFormat& format,
                   int stride,
                   quint8* data,
                   std::unique_ptr<SharedMemory> shared_memory,
                   HBITMAP bitmap)
    : Frame(size, format, stride, data, shared_memory.get()),
      bitmap_(bitmap),
      owned_shared_memory_(std::move(shared_memory))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameDib> FrameDib::create(const Size& size,
                                           const PixelFormat& format,
                                           SharedMemoryFactory* shared_memory_factory,
                                           HDC hdc)
{
    const int bytes_per_row = size.width() * format.bytesPerPixel();
    const size_t buffer_size = calcMemorySize(size, format.bytesPerPixel());

    BitmapInfo bmi;
    memset(&bmi, 0, sizeof(bmi));

    bmi.header.biSize      = sizeof(bmi.header);
    bmi.header.biBitCount  = format.bitsPerPixel();
    bmi.header.biSizeImage = static_cast<DWORD>(buffer_size);
    bmi.header.biPlanes    = 1;
    bmi.header.biWidth     = size.width();
    bmi.header.biHeight    = -size.height();

    if (format.bitsPerPixel() == 32 || format.bitsPerPixel() == 16)
    {
        bmi.header.biCompression = BI_BITFIELDS;

        bmi.u.mask.red   = format.redMax()   << format.redShift();
        bmi.u.mask.green = format.greenMax() << format.greenShift();
        bmi.u.mask.blue  = format.blueMax()  << format.blueShift();
    }
    else
    {
        bmi.header.biCompression = BI_RGB;

        for (quint32 i = 0; i < 256; ++i)
        {
            const quint32 red   = (i >> format.redShift())   & format.redMax();
            const quint32 green = (i >> format.greenShift()) & format.greenMax();
            const quint32 blue  = (i >> format.blueShift())  & format.blueMax();

            bmi.u.color[i].rgbRed   = static_cast<quint8>(red   * 0xFF / format.redMax());
            bmi.u.color[i].rgbGreen = static_cast<quint8>(green * 0xFF / format.greenMax());
            bmi.u.color[i].rgbBlue  = static_cast<quint8>(blue  * 0xFF / format.blueMax());
        }
    }

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
        LOG(LS_ERROR) << "CreateDIBSection failed";
        return nullptr;
    }

    return std::unique_ptr<FrameDib>(new FrameDib(
        size, format, bytes_per_row, reinterpret_cast<quint8*>(data),
        std::move(shared_memory), bitmap));
}

} // namespace base
