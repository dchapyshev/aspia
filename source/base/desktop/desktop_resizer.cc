//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"
#include "base/logging.h"

#if defined(OS_WIN)
#include "base/desktop/desktop_resizer_win.h"
#endif // defined(OS_WIN)

#if defined(OS_LINUX)
#include "base/desktop/desktop_resizer_x11.h"
#endif // defined(OS_LINUX)

namespace base {

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<DesktopResizer> DesktopResizer::create()
{
#if defined(OS_WIN)
    return std::make_unique<DesktopResizerWin>();
#elif defined(OS_LINUX)
    return std::make_unique<DesktopResizerX11>();
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

} // namespace base
