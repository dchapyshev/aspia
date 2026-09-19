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

#include "base/build_config.h"
#include "host/screen_capturer.h"

namespace {

const QString kOrganization = "aspia";
const QString kApplication = "host";

const QString kApplicationShutdown = "application_shutdown";
const QString kPreferredVideoCapturer = "preferred_video_capturer";
const QString kWaylandRestoreToken = "wayland_restore_token";

const QString kUpdateChannel = "update/channel";
const QString kUpdateAutoUpdate = "update/auto_update";
const QString kUpdateCheckFrequency = "update/check_frequency";

} // namespace

//--------------------------------------------------------------------------------------------------
SystemSettings::SystemSettings()
    // The desktop host runs as a system service and keeps these machine-wide (SystemScope). On Android
    // the app is sandboxed and cannot write to the system scope, so use the app-private user scope there
    // (the same split as HostStorage).
#if defined(Q_OS_ANDROID)
    : settings_(QSettings::IniFormat, QSettings::UserScope, kOrganization, kApplication)
#else
    : settings_(QSettings::IniFormat, QSettings::SystemScope, kOrganization, kApplication)
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
QString SystemSettings::updateChannel() const
{
    QString value = settings_.value(kUpdateChannel).toString();
    if (value.isEmpty())
        value = kStableUpdateChannel;

    return value;
}

//--------------------------------------------------------------------------------------------------
void SystemSettings::setUpdateChannel(const QString& channel)
{
    settings_.setValue(kUpdateChannel, channel);
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
