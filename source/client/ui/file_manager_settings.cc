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
    : settings_(QSettings::UserScope, QStringLiteral("Aspia"), QStringLiteral("Client"))
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

QByteArray FileManagerSettings::splitterState() const
{
    return settings_.value(QStringLiteral("FileManager/SplitterState")).toByteArray();
}

void FileManagerSettings::setSplitterState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/SplitterState"), state);
}

QByteArray FileManagerSettings::localDriveListState() const
{
    return settings_.value(QStringLiteral("FileManager/LocalDriveListState")).toByteArray();
}

void FileManagerSettings::setLocalDriveListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/LocalDriveListState"), state);
}

QByteArray FileManagerSettings::localFileListState() const
{
    return settings_.value(QStringLiteral("FileManager/LocalFileListState")).toByteArray();
}

void FileManagerSettings::setLocalFileListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/LocalFileListState"), state);
}

QByteArray FileManagerSettings::remoteDriveListState() const
{
    return settings_.value(QStringLiteral("FileManager/RemoteDriveListState")).toByteArray();
}

void FileManagerSettings::setRemoteDriveListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/RemoteDriveListState"), state);
}

QByteArray FileManagerSettings::remoteFileListState() const
{
    return settings_.value(QStringLiteral("FileManager/RemoteFileListState")).toByteArray();
}

void FileManagerSettings::setRemoteFileListState(const QByteArray& state)
{
    settings_.setValue(QStringLiteral("FileManager/RemoteFileListState"), state);
}

} // namespace aspia
