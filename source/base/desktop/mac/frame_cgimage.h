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

#ifndef BASE_DESKTOP_MAC_FRAME_CGIMAGE_H
#define BASE_DESKTOP_MAC_FRAME_CGIMAGE_H

#include "base/desktop/frame.h"
#include "base/mac/scoped_cftyperef.h"

#include <memory>

#include <CoreGraphics/CoreGraphics.h>

namespace base {

class FrameCGImage final : public Frame
{
public:
    // Create an image containing a snapshot of the display at the time this is being called.
    static std::unique_ptr<FrameCGImage> createForDisplay(CGDirectDisplayID display_id);

    static std::unique_ptr<FrameCGImage> createFromCGImage(ScopedCFTypeRef<CGImageRef> cg_image);

    ~FrameCGImage() final;

private:
    // This constructor expects `cg_image` to hold a non-null CGImageRef.
    FrameCGImage(Size size,
                 int stride,
                 uint8_t* data,
                 ScopedCFTypeRef<CGImageRef> cg_image,
                 ScopedCFTypeRef<CFDataRef> cg_data);

    const ScopedCFTypeRef<CGImageRef> cg_image_;
    const ScopedCFTypeRef<CFDataRef> cg_data_;

    Q_DISABLE_COPY(FrameCGImage)
};

} // namespace base

#endif // BASE_DESKTOP_MAC_FRAME_CGIMAGE_H
