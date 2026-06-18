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

#include "client/session_keeper.h"

#if defined(Q_OS_ANDROID)
#include "client/android/session_keeper_android.h"
#endif // defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
SessionKeeper::SessionKeeper(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
SessionKeeper* SessionKeeper::create(QObject* parent)
{
#if defined(Q_OS_ANDROID)
    return new SessionKeeperAndroid(parent);
#else
    Q_UNUSED(parent)
    return nullptr;
#endif
}
