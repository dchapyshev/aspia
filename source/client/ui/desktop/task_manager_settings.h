//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_TASK_MANAGER_SETTINGS_H
#define CLIENT_UI_DESKTOP_TASK_MANAGER_SETTINGS_H

#include "base/macros_magic.h"

#include <chrono>

#include <QSettings>

namespace client {

class TaskManagerSettings
{
public:
    TaskManagerSettings();

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    QByteArray processColumnState() const;
    void setProcessColumnState(const QByteArray& state);

    QByteArray serviceColumnState() const;
    void setServiceColumnState(const QByteArray& state);

    QByteArray userColumnState() const;
    void setUserColumnState(const QByteArray& state);

    std::chrono::milliseconds updateSpeed() const;
    void setUpdateSpeed(const std::chrono::milliseconds& speed);

private:
    QSettings settings_;

    DISALLOW_COPY_AND_ASSIGN(TaskManagerSettings);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_TASK_MANAGER_SETTINGS_H
