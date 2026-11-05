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

#ifndef HOST_BANDWIDTH_CONTROLLER_H
#define HOST_BANDWIDTH_CONTROLLER_H

#include <QtClassHelperMacros>
#include <QtTypes>

#include "base/time_types.h"
#include "proto/desktop_internal.h"

class BandwidthController
{
public:
    BandwidthController() = default;
    virtual ~BandwidthController() = default;

    // The session sends the padding of a test in slices this often.
    static constexpr MilliSeconds kPaddingSlice{ 100 };

    virtual void start() = 0;

    // Returns true when a probe has to be sent now, a probe interval after the previous one. Only one
    // probe is in flight at a time. The session asks on every slice of the padding before sending
    // it, so the probe waits behind the queue of the path and not behind a slice.
    virtual bool startProbe(TimePoint now) = 0;
    virtual void finishProbe(TimePoint now) = 0;

    virtual void onBytesSent(qint64 bytes) = 0;

    // Padding the session has to send by |now| for the rate under test, zero when there is none.
    virtual qint64 paddingDue(TimePoint now) const = 0;

    virtual void tick(TimePoint now) = 0;

    virtual qint64 bandwidth() const = 0;
    virtual proto::desktop::Overflow::State overflowState(TimePoint now) const = 0;

private:
    Q_DISABLE_COPY_MOVE(BandwidthController)
};

#endif // HOST_BANDWIDTH_CONTROLLER_H
