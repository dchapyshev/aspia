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

#include "host/system_settings.h"

#include "base/xml_settings.h"
#include "build/build_config.h"
#include "host/screen_capturer.h"

namespace {

const QString kOrganization = "aspia";
const QString kApplication = "host";

const QString kApplicationShutdown = "application_shutdown";
const QString kPreferredVideoCapturer = "preferred_video_capturer";
const QString kVtResolution = "vt_resolution";
const QString kWaylandRestoreToken = "wayland_restore_token";

const QString kUpdateServer = "update/server";
const QString kUpdateAutoUpdate = "update/auto_update";
const QString kUpdateCheckFrequency = "update/check_frequency";

} // namespace

//--------------------------------------------------------------------------------------------------
SystemSettings::SystemSettings()
    // The desktop host runs as a system service and keeps these machine-wide (SystemScope). On Android
    // the app is sandboxed and cannot write to the system scope, so use the app-private user scope there
    // (the same split as HostStorage).
#if defined(Q_OS_ANDROID)
    : settings_(XmlSettings::format(), QSettings::UserScope, kOrganization, kApplication)
#else
    : settings_(XmlSettings::format(), QSettings::SystemScope, kOrganization, kApplication)
#endif
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SystemSettings::~SystemSettings() = default;

//--------------------------------------------------------------------------------------------------
QString SystemSettings::filePath() const
{
    return settings_.fileName();
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isWritable() const
{
    return settings_.isWritable();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::sync()
{
    settings_.sync();
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::updateServer() const
{
    return settings_.value(kUpdateServer, QString(DEFAULT_UPDATE_SERVER)).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServer, server);
}

//--------------------------------------------------------------------------------------------------
quint32 SystemSettings::preferredVideoCapturer() const
{
    return settings_.value(
        kPreferredVideoCapturer, static_cast<quint32>(ScreenCapturer::Type::UNKNOWN)).toUInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setPreferredVideoCapturer(quint32 type)
{
    settings_.setValue(kPreferredVideoCapturer, type);
}

//--------------------------------------------------------------------------------------------------
QSize SystemSettings::vtResolution() const
{
    return settings_.value(kVtResolution).toSize();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setVtResolution(const QSize& resolution)
{
    settings_.setValue(kVtResolution, resolution);
}

//--------------------------------------------------------------------------------------------------
QString SystemSettings::waylandRestoreToken() const
{
    return settings_.value(kWaylandRestoreToken).toString();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setWaylandRestoreToken(const QString& token)
{
    settings_.setValue(kWaylandRestoreToken, token);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isApplicationShutdownDisabled() const
{
    return settings_.value(kApplicationShutdown, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setApplicationShutdownDisabled(bool value)
{
    settings_.setValue(kApplicationShutdown, value);
}

//--------------------------------------------------------------------------------------------------
bool SystemSettings::isAutoUpdateEnabled() const
{
    return settings_.value(kUpdateAutoUpdate, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setAutoUpdateEnabled(bool enable)
{
    settings_.setValue(kUpdateAutoUpdate, enable);
}

//--------------------------------------------------------------------------------------------------
int SystemSettings::updateCheckFrequency() const
{
    return settings_.value(kUpdateCheckFrequency, 7).toInt();
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateCheckFrequency(int days)
{
    settings_.setValue(kUpdateCheckFrequency, days);
}
