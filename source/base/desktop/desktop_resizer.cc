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

#include "base/desktop/desktop_resizer.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/desktop/desktop_resizer_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "base/desktop/desktop_resizer_x11.h"
#endif // defined(Q_OS_LINUX)

namespace base {

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<DesktopResizer> DesktopResizer::create()
{
#if defined(Q_OS_WINDOWS)
    return std::make_unique<DesktopResizerWin>();
#elif defined(Q_OS_LINUX)
    return std::make_unique<DesktopResizerX11>();
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

} // namespace base
