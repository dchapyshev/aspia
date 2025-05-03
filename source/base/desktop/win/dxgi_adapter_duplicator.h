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

#ifndef BASE_DESKTOP_WIN_DXGI_ADAPTER_DUPLICATOR_H
#define BASE_DESKTOP_WIN_DXGI_ADAPTER_DUPLICATOR_H

#include "base/desktop/shared_frame.h"
#include "base/desktop/win/d3d_device.h"
#include "base/desktop/win/dxgi_context.h"
#include "base/desktop/win/dxgi_output_duplicator.h"

#include <vector>

namespace base {

// A container of DxgiOutputDuplicators to duplicate monitors attached to a single video card.
class DxgiAdapterDuplicator
{
public:
    using Context = DxgiAdapterContext;

    // Creates an instance of DxgiAdapterDuplicator from a D3dDevice. Only
    // DxgiDuplicatorController can create an instance.
    explicit DxgiAdapterDuplicator(const D3dDevice& device);

    // Move constructor, to make it possible to store instances of DxgiAdapterDuplicator in
    // std::vector<>.
    DxgiAdapterDuplicator(DxgiAdapterDuplicator&& other);

    ~DxgiAdapterDuplicator();

    enum class ErrorCode
    {
        SUCCESS,
        CRITICAL_ERROR,
        GENERIC_ERROR
    };

    // Initializes the DxgiAdapterDuplicator from a D3dDevice.
    ErrorCode initialize();

    // Sequentially calls Duplicate function of all the DxgiOutputDuplicator instances owned by
    // this instance, and writes into |target|.
    bool duplicate(Context* context, SharedFrame* target, DxgiCursor* cursor);

    // Captures one monitor and writes into |target|. |monitor_id| should be between [0, screenCount()).
    bool duplicateMonitor(Context* context, int monitor_id, SharedFrame* target, DxgiCursor* cursor);

    // Returns desktop rect covered by this DxgiAdapterDuplicator.
    const Rect& desktopRect() const { return desktop_rect_; }

    // Returns the size of one screen owned by this DxgiAdapterDuplicator. |id| should be between
    // [0, screenCount()).
    Rect screenRect(int id) const;

    // Returns the device name of one screen owned by this DxgiAdapterDuplicator. |id| should be
    // between [0, screenCount()).
    const QString& deviceName(int id) const;

    // Returns the count of screens owned by this DxgiAdapterDuplicator. These screens can be
    // retrieved by an interger in the range of [0, screenCount()).
    int screenCount() const;

    void setup(Context* context);

    void unregister(const Context* const context);

    // The minimum num_frames_captured() returned by |duplicators_|.
    qint64 numFramesCaptured() const;

    // Moves |desktop_rect_| and all underlying |duplicators_|. See
    // DxgiDuplicatorController::translateRect().
    void translateRect(const Point& position);

private:
    ErrorCode doInitialize();

    const D3dDevice device_;
    std::vector<DxgiOutputDuplicator> duplicators_;
    Rect desktop_rect_;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_ADAPTER_DUPLICATOR_H
