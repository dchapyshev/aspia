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

#include "base/service_controller.h"

#if defined(Q_OS_WINDOWS)
#include "base/service_controller_win.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "base/service_controller_systemd.h"
#endif // defined(Q_OS_LINUX)

namespace base {

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceController::open(const QString& name)
{
#if defined(Q_OS_WINDOWS)
    return ServiceControllerWin::open(name);
#elif defined(Q_OS_LINUX)
    return ServiceControllerSystemd::open(name);
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceController::install(
    const QString& name, const QString& display_name, const QString& file_path)
{
#if defined(Q_OS_WINDOWS)
    return ServiceControllerWin::install(name, display_name, file_path);
#elif defined(Q_OS_LINUX)
    return ServiceControllerSystemd::install(name, display_name, file_path);
#else
    NOTIMPLEMENTED();
    return nullptr;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceController::remove(const QString& name)
{
#if defined(Q_OS_WINDOWS)
    return ServiceControllerWin::remove(name);
#elif defined(Q_OS_LINUX)
    return ServiceControllerSystemd::remove(name);
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceController::isInstalled(const QString& name)
{
#if defined(Q_OS_WINDOWS)
    return ServiceControllerWin::isInstalled(name);
#elif defined(Q_OS_LINUX)
    return ServiceControllerSystemd::isInstalled(name);
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceController::isRunning(const QString& name)
{
#if defined(Q_OS_WINDOWS)
    return ServiceControllerWin::isRunning(name);
#elif defined(Q_OS_LINUX)
    return ServiceControllerSystemd::isRunning(name);
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

} // namespace base
