//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace aspia {

FileManagerSettings::FileManagerSettings()
    : settings_(QSettings::UserScope, QStringLiteral("aspia"), QStringLiteral("client"))
{
    // Nothing
}

QByteArray FileManagerSettings::windowGeometry() const
{
    return settings_.value(QStringLiteral("file_manager/window_geometry")).toByteArray();
}

void FileManagerSettings::setWindowGeometry(const QByteArray& geometry)
{
    settings_.setValue(QStringLiteral("file_manager/window_geometry"), geometry);
}

QByteArray FileManagerSettings::splitterState() const
{
    return settings_.value(QStringLiteral("file_manager/splitter_state")).toByteArray();
}

void FileManagerSettings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("file_manager/splitter_state"), state);
}

QByteArray FileManagerSettings::localDriveListState() const
{
    return settings_.value(QStringLiteral("file_manager/local_drive_list_state")).toByteArray();
}

void FileManagerSettings::setLocalDriveListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("file_manager/local_drive_list_state"), state);
}

QByteArray FileManagerSettings::localFileListState() const
{
    return settings_.value(QStringLiteral("file_manager/local_file_list_state")).toByteArray();
}

void FileManagerSettings::setLocalFileListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("file_manager/local_file_list_state"), state);
}

QByteArray FileManagerSettings::remoteDriveListState() const
{
    return settings_.value(QStringLiteral("file_manager/remote_drive_list_state")).toByteArray();
}

void FileManagerSettings::setRemoteDriveListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("file_manager/remote_drive_list_state"), state);
}

QByteArray FileManagerSettings::remoteFileListState() const
{
    return settings_.value(QStringLiteral("file_manager/remote_file_list_state")).toByteArray();
}

void FileManagerSettings::setRemoteFileListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("file_manager/remote_file_list_state"), state);
}

} // namespace aspia
