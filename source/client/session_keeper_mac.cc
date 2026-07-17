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

#include "client/session_keeper_mac.h"

#include "base/mac/app_nap_blocker.h"

//--------------------------------------------------------------------------------------------------
SessionKeeperMac::SessionKeeperMac(QObject* parent)
    : SessionKeeper(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SessionKeeperMac::~SessionKeeperMac()
{
    release();
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperMac::acquire()
{
    if (blocked_)
        return;

    addAppNapBlock();
    blocked_ = true;
}

//--------------------------------------------------------------------------------------------------
void SessionKeeperMac::release()
{
    if (!blocked_)
        return;

    releaseAppNapBlock();
    blocked_ = false;
}
