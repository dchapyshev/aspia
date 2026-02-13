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

#ifndef BASE_DESKTOP_WIN_DXGI_OUTPUT_DUPLICATOR_H
#define BASE_DESKTOP_WIN_DXGI_OUTPUT_DUPLICATOR_H

#include <QVector>

#include "base/shared_pointer.h"
#include "base/desktop/frame_rotation.h"
#include "base/desktop/win/d3d_device.h"
#include "base/desktop/win/dxgi_context.h"
#include "base/desktop/win/dxgi_cursor.h"
#include "base/desktop/win/dxgi_texture.h"
#include "base/win/desktop.h"

#include <dxgi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace base {

// Duplicates the content on one IDXGIOutput, i.e. one monitor attached to one video card. None of
// functions in this class is thread-safe.
class DxgiOutputDuplicator
{
public:
    using Context = DxgiOutputContext;

    // Creates an instance of DxgiOutputDuplicator from a D3dDevice and one of its IDXGIOutput1.
    // Caller must maintain the lifetime of device, to make sure it outlives this instance. Only
    // DxgiAdapterDuplicator can create an instance.
    DxgiOutputDuplicator(const D3dDevice& device,
                         const Microsoft::WRL::ComPtr<IDXGIOutput1>& output,
                         const DXGI_OUTPUT_DESC& desc);

    // To allow this class to work with vector.
    DxgiOutputDuplicator(DxgiOutputDuplicator&& other);

    // Destructs this instance. We need to make sure texture_ has been released before duplication_.
    ~DxgiOutputDuplicator();

    // Initializes duplication object.
    bool initialize();

    // Copies the content of current IDXGIOutput to the |target|. To improve the performance, this
    // function copies only regions merged from |context|->updated_region and detectUpdatedRegion().
    // The |offset| decides the offset in the |target| where the content should be copied to. i.e.
    // this function copies the content to the rectangle of (offset.x(), offset.y()) to
    // (offset.x() + desktop_rect_.width(), offset.y() + desktop_rect_.height()).
    // Returns false in case of a failure.
    bool duplicate(Context* context, const QPoint& offset, SharedPointer<Frame>& target_frame, DxgiCursor* cursor);

    // Returns the desktop rect covered by this DxgiOutputDuplicator.
    const QRect& desktopRect() const { return desktop_rect_; }

    // Returns the device name from DXGI_OUTPUT_DESC.
    const QString& deviceName() const { return device_name_; }

    void setup(Context* context);

    void unregister(const Context* const context);

    // How many frames have been captured by this DxigOutputDuplicator.
    qint64 numFramesCaptured() const;

    // Moves |desktop_rect_|. See DxgiDuplicatorController::translateRect().
    void translateRect(const QPoint& position);

private:
    // Calls doDetectUpdatedRegion(). If it fails, this function sets the |updated_region| as
    // entire untranslatedDesktopRect().
    void detectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info, QRegion* updated_region);

    // Returns untranslated updated region, which are directly returned by Windows APIs. Returns
    // false in case of a failure.
    bool doDetectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info, QRegion* updated_region);

    bool releaseFrame();

    // Initializes duplication_ instance. Expects duplication_ is in empty status.
    // Returns false if system does not support IDXGIOutputDuplication.
    bool duplicateOutput();

    // Returns a Rect with the same size of desktopSize(), but translated by offset.
    QRect translatedDesktopRect(const QPoint& offset) const;

    // Returns a Rect with the same size of desktopSize(), but starts from (0, 0).
    QRect untranslatedDesktopRect() const;

    // Spreads changes from |context| to other registered Context(s) in contexts_.
    void spreadContextChange(const Context* const context);

    // Returns the size of desktop rectangle current instance representing.
    QSize desktopSize() const;

    const D3dDevice device_;
    const Microsoft::WRL::ComPtr<IDXGIOutput1> output_;
    const QString device_name_;
    const QRect initial_desktop_rect_;
    QRect desktop_rect_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    DXGI_OUTDUPL_DESC desc_;
    QVector<quint8> metadata_;
    std::unique_ptr<DxgiTexture> texture_;
    Rotation rotation_ = Rotation::CLOCK_WISE_0;
    QSize unrotated_size_;

    // After each AcquireNextFrame() function call, updated_region_(s) of all active Context(s)
    // need to be updated. Since they have missed the change this time. And during next duplicate()
    // function call, their updated_region_ will be merged and copied.
    QVector<Context*> contexts_;

    qint64 num_frames_captured_ = 0;
    Desktop desktop_;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_OUTPUT_DUPLICATOR_H
