//
// SmartCafe Project
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

#include "client/ui/desktop/task_manager_settings.h"

#include <QStandardPaths>

namespace client {

namespace {

const QString kWindowStateParam = QStringLiteral("TaskManager/WindowState");
const QString kProcessColumnStateParam = QStringLiteral("TaskManager/ProcessColumnState");
const QString kServiceColumnStateParam = QStringLiteral("TaskManager/ServiceColumnState");
const QString kUserColumnStateParam = QStringLiteral("TaskManager/UserColumnState");
const QString kUpdateSpeedParam = QStringLiteral("TaskManager/UpdateSpeed");

} // namespace

//--------------------------------------------------------------------------------------------------
TaskManagerSettings::TaskManagerSettings()
    : settings_(QSettings::IniFormat,
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QByteArray TaskManagerSettings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void TaskManagerSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray TaskManagerSettings::processColumnState() const
{
    return settings_.value(kProcessColumnStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void TaskManagerSettings::setProcessColumnState(const QByteArray& state)
{
    settings_.setValue(kProcessColumnStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray TaskManagerSettings::serviceColumnState() const
{
    return settings_.value(kServiceColumnStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void TaskManagerSettings::setServiceColumnState(const QByteArray& state)
{
    settings_.setValue(kServiceColumnStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QByteArray TaskManagerSettings::userColumnState() const
{
    return settings_.value(kUserColumnStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void TaskManagerSettings::setUserColumnState(const QByteArray& state)
{
    settings_.setValue(kUserColumnStateParam, state);
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds TaskManagerSettings::updateSpeed() const
{
    return std::chrono::milliseconds(settings_.value(kUpdateSpeedParam, 1000).toInt());
}

//--------------------------------------------------------------------------------------------------
void TaskManagerSettings::setUpdateSpeed(const std::chrono::milliseconds& speed)
{
    settings_.setValue(kUpdateSpeedParam, static_cast<int>(speed.count()));
}

} // namespace client
