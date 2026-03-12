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

#include "base/service_controller_systemd.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ServiceControllerSystemd::ServiceControllerSystemd()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerSystemd::~ServiceControllerSystemd() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerSystemd::open(const QString& name)
{
    NOTIMPLEMENTED();
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerSystemd::install(
    const QString& name, const QString& display_name, const QString& file_path)
{
    NOTIMPLEMENTED();
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::remove(const QString& name)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::isInstalled(const QString& name)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerSystemd::isRunning(const QString& name)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setDescription(const QString& description)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerSystemd::description() const
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setDependencies(const QStringList& dependencies)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
QStringList ServiceControllerSystemd::dependencies() const
{
    NOTIMPLEMENTED();
    return QStringList();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::setAccount(const QString& username, const QString& password)
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerSystemd::filePath() const
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::isRunning() const
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::start()
{
    NOTIMPLEMENTED();
    return false;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerSystemd::stop()
{
    NOTIMPLEMENTED();
    return false;
}

} // namespace base
