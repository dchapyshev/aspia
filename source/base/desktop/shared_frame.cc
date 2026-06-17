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

#include "base/desktop/shared_frame.h"

#include "base/logging.h"
#include "base/desktop/frame.h"

namespace {

struct Registrator
{
    Registrator() { qRegisterMetaType<SharedFrame>("SharedFrame"); }
};

static volatile Registrator registrator;

} // namespace

struct SharedFrame::Core
{
    explicit Core(std::unique_ptr<Frame> frame)
        : frame(std::move(frame))
    {
        // Nothing else.
    }

    const std::unique_ptr<Frame> frame;
    std::mutex mutex;
};

//--------------------------------------------------------------------------------------------------
SharedFrame::SharedFrame(std::unique_ptr<Frame> frame)
    : core_(std::make_shared<Core>(std::move(frame)))
{
    DCHECK(core_->frame);
}

//--------------------------------------------------------------------------------------------------
QSize SharedFrame::size() const
{
    return core_ ? core_->frame->size() : QSize();
}

//--------------------------------------------------------------------------------------------------
SharedFrame::ReadAccess SharedFrame::read() const
{
    return ReadAccess(core_->mutex, *core_->frame);
}

//--------------------------------------------------------------------------------------------------
SharedFrame::WriteAccess SharedFrame::write()
{
    return WriteAccess(core_->mutex, *core_->frame);
}
