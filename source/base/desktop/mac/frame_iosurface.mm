//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/mac/frame_iosurface.h"

namespace base {

namespace {

const int kBytesPerPixel = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameIOSurface> FrameIOSurface::wrap(ScopedCFTypeRef<IOSurfaceRef> io_surface)
{
    if (!io_surface)
        return nullptr;

    IOSurfaceIncrementUseCount(io_surface.get());
    IOReturn status = IOSurfaceLock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
    if (status != kIOReturnSuccess)
    {
        LOG(LS_ERROR) << "Failed to lock the IOSurface with status " << status;
        IOSurfaceDecrementUseCount(io_surface.get());
        return nullptr;
    }

    // Verify that the image has 32-bit depth.
    int bytes_per_pixel = IOSurfaceGetBytesPerElement(io_surface.get());
    if (bytes_per_pixel != kBytesPerPixel)
    {
        LOG(LS_ERROR) << "CGDisplayStream handler returned IOSurface with " << (8 * bytes_per_pixel)
                      << " bits per pixel. Only 32-bit depth is supported.";
        IOSurfaceUnlock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
        IOSurfaceDecrementUseCount(io_surface.get());
        return nullptr;
    }

    return std::unique_ptr<FrameIOSurface>(new FrameIOSurface(io_surface));
}

//--------------------------------------------------------------------------------------------------
FrameIOSurface::FrameIOSurface(ScopedCFTypeRef<IOSurfaceRef> io_surface)
    : Frame(Size(IOSurfaceGetWidth(io_surface.get()), IOSurfaceGetHeight(io_surface.get())),
            PixelFormat::ARGB(),
            IOSurfaceGetBytesPerRow(io_surface.get()),
            static_cast<uint8_t*>(IOSurfaceGetBaseAddress(io_surface.get())),
            nullptr),
      io_surface_(io_surface)
{
    DCHECK(io_surface_);
}

//--------------------------------------------------------------------------------------------------
FrameIOSurface::~FrameIOSurface()
{
    IOSurfaceUnlock(io_surface_.get(), kIOSurfaceLockReadOnly, nullptr);
    IOSurfaceDecrementUseCount(io_surface_.get());
}

} // namespace base
