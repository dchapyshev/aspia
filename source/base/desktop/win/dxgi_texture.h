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

#ifndef BASE_DESKTOP_WIN_DXGI_TEXTURE_H
#define BASE_DESKTOP_WIN_DXGI_TEXTURE_H

#include "base/desktop/frame.h"

#include <memory>

#include <d3d11.h>
#include <dxgi1_2.h>

namespace base {

// A texture copied or mapped from a DXGI_OUTDUPL_FRAME_INFO and IDXGIResource.
class DxgiTexture
{
public:
    // Creates a DxgiTexture instance, which represents the |desktop_size| area of
    // entire screen -- usually a monitor on the system.
    DxgiTexture();
    virtual ~DxgiTexture();

    // Copies selected regions of a frame represented by frame_info and resource.
    // Returns false if anything wrong.
    bool copyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info, IDXGIResource* resource);

    const Size& desktopSize() const { return desktop_size_; }
    quint8* bits() const { return static_cast<quint8*>(rect_.pBits); }
    int pitch() const { return static_cast<int>(rect_.Pitch); }

    // Releases the resource currently holds by this instance. Returns false if
    // anything wrong, and this instance should be deprecated in this state. bits,
    // pitch and AsDesktopFrame are only valid after a success CopyFrom() call,
    // but before Release() call.
    bool release();

    // Returns a DesktopFrame snapshot of a DxgiTexture instance. This
    // DesktopFrame is used to copy a DxgiTexture content to another DesktopFrame
    // only. And it should not outlive its DxgiTexture instance.
    const Frame& asDesktopFrame();

protected:
    DXGI_MAPPED_RECT* rect();

    virtual bool copyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                 ID3D11Texture2D* texture) = 0;

    virtual bool doRelease() = 0;

private:
    DXGI_MAPPED_RECT rect_ = { 0 };
    Size desktop_size_;
    std::unique_ptr<Frame> frame_;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_TEXTURE_H
