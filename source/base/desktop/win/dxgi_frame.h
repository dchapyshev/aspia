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

#ifndef BASE__DESKTOP__WIN__DXGI_FRAME_H
#define BASE__DESKTOP__WIN__DXGI_FRAME_H

#include "base/desktop/screen_capturer.h"
#include "base/desktop/shared_frame.h"
#include "base/desktop/win/dxgi_context.h"

#include <memory>
#include <optional>
#include <vector>

namespace base {

class DxgiDuplicatorController;
class SharedMemoryFactory;

// A pair of a SharedFrame and a DxgiDuplicatorController::Context for the client of
// DxgiDuplicatorController.
class DxgiFrame final
{
public:
    using Context = DxgiFrameContext;

    explicit DxgiFrame(std::shared_ptr<DxgiDuplicatorController> controller,
                       base::SharedMemoryFactory* shared_memory_factory);
    ~DxgiFrame();

    // Should not be called if prepare() is not executed or returns false.
    SharedFrame* frame() const;

private:
    // Allows DxgiDuplicatorController to access prepare() and context() function as well as
    // Context class.
    friend class DxgiDuplicatorController;

    // Prepares current instance with desktop size and source id.
    bool prepare(const Size& size, ScreenCapturer::ScreenId source_id);

    // Should not be called if prepare() is not executed or returns false.
    Context* context();

    base::SharedMemoryFactory* const shared_memory_factory_;
    std::optional<Size> last_frame_size_;
    ScreenCapturer::ScreenId source_id_ = ScreenCapturer::kFullDesktopScreenId;
    std::unique_ptr<SharedFrame> frame_;
    Context context_;
};

} // namespace base

#endif // BASE__DESKTOP__WIN__DXGI_FRAME_H
