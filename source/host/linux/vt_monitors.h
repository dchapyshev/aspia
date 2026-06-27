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

#ifndef HOST_LINUX_VT_MONITORS_H
#define HOST_LINUX_VT_MONITORS_H

#include <utility>
#include <vector>

#include "base/scoped_qpointer.h"
#include "host/linux/vt_session.h"

// A set of virtual terminals presented to the client as switchable monitors. One instance is shared by the
// capturer, input injector and resizer: the capturer selects the active terminal, the injector sends input
// to it, and the resizer addresses any terminal by index.
class VtMonitors
{
public:
    explicit VtMonitors(std::vector<ScopedQPointer<VtSession>> sessions)
        : sessions_(std::move(sessions))
    {
        // Nothing
    }

    int count() const { return static_cast<int>(sessions_.size()); }

    int active() const { return active_; }
    void setActive(int index)
    {
        if (index >= 0 && index < count())
            active_ = index;
    }

    VtSession* session(int index) const
    {
        return (index >= 0 && index < count()) ? sessions_[index].get() : nullptr;
    }

    VtSession* activeSession() const { return session(active_); }

private:
    std::vector<ScopedQPointer<VtSession>> sessions_;
    int active_ = 0;

    VtMonitors(const VtMonitors&) = delete;
    VtMonitors& operator=(const VtMonitors&) = delete;
};

#endif // HOST_LINUX_VT_MONITORS_H
