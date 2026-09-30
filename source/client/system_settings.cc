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

#include "client/system_settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "base/logging.h"
#include "base/files/base_paths.h"

namespace {

const QString kUpdateServerParam = "update/address";
const QString kUpdatePublicKeyParam = "update/public_key";

//--------------------------------------------------------------------------------------------------
QString filePath()
{
#if defined(Q_OS_WINDOWS)
    // Only administrators can write next to the installed application.
    return QCoreApplication::applicationDirPath() + "/client.ini";
#else
    return BasePaths::appConfigDir() + "/client.ini";
#endif
}

} // namespace

//--------------------------------------------------------------------------------------------------
SystemSettings::SystemSettings()
    : settings_(filePath(), QSettings::IniFormat)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SystemSettings::~SystemSettings() = default;

//--------------------------------------------------------------------------------------------------
bool SystemSettings::sync()
{
    const QString file_path = settings_.fileName();

    if (!QDir().mkpath(QFileInfo(file_path).absolutePath()))
    {
        LOG(ERROR) << "Unable to create the directory for" << file_path;
        return false;
    }

    settings_.sync();
    if (settings_.status() != QSettings::NoError)
    {
        LOG(ERROR) << "Unable to write" << file_path;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::updateServer() const
{
    return settings_.value(kUpdateServerParam).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServerParam, server);
}

//--------------------------------------------------------------------------------------------------
QByteArray SystemSettings::updatePublicKey() const
{
    return QByteArray::fromHex(settings_.value(kUpdatePublicKeyParam).toString().toLatin1());
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdatePublicKey(const QByteArray& public_key)
{
    settings_.setValue(kUpdatePublicKeyParam, QString::fromLatin1(public_key.toHex()));
}
