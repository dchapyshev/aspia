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

#include "desktop/shared_desktop_frame.h"

#include <memory>
#include <type_traits>
#include <utility>

namespace desktop {

SharedFrame::~SharedFrame() = default;

// static
std::unique_ptr<SharedFrame> SharedFrame::wrap(std::unique_ptr<Frame> desktop_frame)
{
    return std::unique_ptr<SharedFrame>(new SharedFrame(new Core(std::move(desktop_frame))));
}

bool SharedFrame::shareFrameWith(const SharedFrame& other) const
{
    return core_->get() == other.core_->get();
}

std::unique_ptr<SharedFrame> SharedFrame::share()
{
    std::unique_ptr<SharedFrame> result(new SharedFrame(core_));
    result->copyFrameInfoFrom(*this);
    return result;
}

bool SharedFrame::isShared()
{
    return !core_->hasOneRef();
}

SharedFrame::SharedFrame(base::scoped_refptr<Core> core)
    : Frame((*core)->size(), (*core)->format(), (*core)->stride(), (*core)->frameData()),
      core_(core)
{
    copyFrameInfoFrom(*(core_)->get());
}

} // namespace desktop
