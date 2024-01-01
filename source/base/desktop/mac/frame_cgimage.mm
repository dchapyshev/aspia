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

#include "base/desktop/mac/frame_cgimage.h"

#include <AvailabilityMacros.h>

namespace base {

namespace {

const int kBytesPerPixel = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameCGImage> FrameCGImage::createForDisplay(CGDirectDisplayID display_id)
{
    // Create an image containing a snapshot of the display.
    ScopedCFTypeRef<CGImageRef> cg_image(CGDisplayCreateImage(display_id));
    if (!cg_image)
        return nullptr;

    return FrameCGImage::createFromCGImage(cg_image);
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameCGImage> FrameCGImage::createFromCGImage(ScopedCFTypeRef<CGImageRef> cg_image)
{
    // Verify that the image has 32-bit depth.
    int bits_per_pixel = CGImageGetBitsPerPixel(cg_image.get());
    if (bits_per_pixel / 8 != kBytesPerPixel)
    {
        LOG(LS_ERROR) << "CGDisplayCreateImage() returned imaged with " << bits_per_pixel
                      << " bits per pixel. Only 32-bit depth is supported.";
        return nullptr;
    }

    // Request access to the raw pixel data via the image's DataProvider.
    CGDataProviderRef cg_provider = CGImageGetDataProvider(cg_image.get());
    DCHECK(cg_provider);

    // CGDataProviderCopyData returns a new data object containing a copy of the provider’s data.
    ScopedCFTypeRef<CFDataRef> cg_data(CGDataProviderCopyData(cg_provider));
    DCHECK(cg_data);

    // CFDataGetBytePtr returns a read-only pointer to the bytes of a CFData object.
    uint8_t* data = const_cast<uint8_t*>(CFDataGetBytePtr(cg_data.get()));
    DCHECK(data);

    Size size(CGImageGetWidth(cg_image.get()), CGImageGetHeight(cg_image.get()));
    int stride = CGImageGetBytesPerRow(cg_image.get());

    std::unique_ptr<FrameCGImage> frame(new FrameCGImage(size, stride, data, cg_image, cg_data));
    return frame;
}

//--------------------------------------------------------------------------------------------------
FrameCGImage::FrameCGImage(Size size,
                           int stride,
                           uint8_t* data,
                           ScopedCFTypeRef<CGImageRef> cg_image,
                           ScopedCFTypeRef<CFDataRef> cg_data)
    : Frame(size, PixelFormat::ARGB(), stride, data, nullptr),
      cg_image_(cg_image),
      cg_data_(cg_data)
{
    DCHECK(cg_image_);
    DCHECK(cg_data_);
}

//--------------------------------------------------------------------------------------------------
FrameCGImage::~FrameCGImage() = default;

}  // namespace webrtc
