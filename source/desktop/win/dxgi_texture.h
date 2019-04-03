//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__WIN__DXGI_TEXTURE_H
#define DESKTOP__WIN__DXGI_TEXTURE_H

#include "desktop/desktop_frame.h"

#include <QSize>

#include <memory>

#include <D3D11.h>
#include <DXGI1_2.h>

namespace desktop {

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

    const QSize& desktopSize() const { return desktop_size_; }
    uint8_t* bits() const { return static_cast<uint8_t*>(rect_.pBits); }
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
    QSize desktop_size_;
    std::unique_ptr<Frame> frame_;
};

} // namespace desktop

#endif // DESKTOP__WIN__DXGI_TEXTURE_H
