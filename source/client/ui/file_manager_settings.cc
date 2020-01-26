//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_manager_settings.h"

namespace client {

FileManagerSettings::FileManagerSettings()
    : settings_(QSettings::UserScope, QStringLiteral("aspia"), QStringLiteral("client"))
{
    // Nothing
}

QByteArray FileManagerSettings::windowGeometry() const
{
    return settings_.value(QStringLiteral("FileManager/WindowGeometry")).toByteArray();
}

void FileManagerSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("FileManager/WindowGeometry"), geometry);
}

QByteArray FileManagerSettings::windowState() const
{
    return settings_.value(QStringLiteral("FileManager/WindowState")).toByteArray();
}

void FileManagerSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/WindowState"), state);
}

} // namespace client
