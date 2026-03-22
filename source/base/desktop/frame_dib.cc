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

#include "base/desktop/frame_dib.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
FrameDib::FrameDib(const QSize& size, int stride, quint8* data, HBITMAP bitmap)
    : Frame(size, stride, data),
      bitmap_(bitmap)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameDib> FrameDib::create(const QSize& size, HDC hdc)
{
    const int bytes_per_row = size.width() * kBytesPerPixel;
    const DWORD buffer_size = static_cast<DWORD>(
        ((size.width() + 128 * 2) * (size.height() + 128 * 2)) * kBytesPerPixel);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));

    bmi.bmiHeader.biSize      = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biBitCount  = kBytesPerPixel * 8;
    bmi.bmiHeader.biSizeImage = buffer_size;
    bmi.bmiHeader.biPlanes    = 1;
    bmi.bmiHeader.biWidth     = size.width();
    bmi.bmiHeader.biHeight    = -size.height();

    void* data = nullptr;
    HBITMAP bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &data, nullptr, 0);
    if (!bitmap)
    {
        LOG(ERROR) << "CreateDIBSection failed";
        return nullptr;
    }

    return std::unique_ptr<FrameDib>(new FrameDib(
        size, bytes_per_row, reinterpret_cast<quint8*>(data), bitmap));
}

} // namespace base
