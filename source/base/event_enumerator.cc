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

#include "base/event_enumerator.h"

#if defined(Q_OS_WINDOWS)
#include "base/event_enumerator_win.h"
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "base/event_enumerator_linux.h"
#endif

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<EventEnumerator> EventEnumerator::create(
    const QString& log_name, quint32 start, quint32 count)
{
#if defined(Q_OS_WINDOWS)
    return std::make_unique<EventEnumeratorWin>(log_name, start, count);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    return std::make_unique<EventEnumeratorLinux>(log_name, start, count);
#else
    return nullptr;
#endif
}
