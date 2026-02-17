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

#include "client/ui/file_transfer/file_manager_settings.h"

#include "base/xml_settings.h"

namespace client {

namespace {

const QString kWindowGeometryParam = QStringLiteral("FileManager/WindowGeometry");
const QString kWindowStateParam = QStringLiteral("FileManager/WindowState");

} // namespace

//--------------------------------------------------------------------------------------------------
FileManagerSettings::FileManagerSettings()
    : settings_(base::XmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QByteArray FileManagerSettings::windowGeometry() const
{
    return settings_.value(kWindowGeometryParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void FileManagerSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(kWindowGeometryParam, geometry);
}

//--------------------------------------------------------------------------------------------------
QByteArray FileManagerSettings::windowState() const
{
    return settings_.value(kWindowStateParam).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void FileManagerSettings::setWindowState(const QByteArray& state)
{
    settings_.setValue(kWindowStateParam, state);
}

} // namespace client
