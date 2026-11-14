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

#include "client/storage/user_settings.h"

#include <QLocale>

#include "base/build_config.h"
#include "proto/peer.h"

namespace {

const QString kLocaleParam = "locale";
const QString kThemeParam = "theme";
const QString kLastDatabaseParam = "last_database";
const QString kConfirmCloseParam = "confirm_close";
const QString kCheckUpdatesParam = "check_updates";
const QString kUpdateChannelParam = "update_channel";
const QString kMcpEnabledParam = "mcp_enabled";
const QString kMcpAutoConfirmParam = "mcp_auto_confirm";
const QString kMcpSignInParam = "mcp_sign_in";
const QString kMcpSessionTypesParam = "mcp_session_types";

} // namespace

//--------------------------------------------------------------------------------------------------
UserSettings::UserSettings()
    : settings_(QSettings::IniFormat, QSettings::UserScope, "aspia", "client")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
UserSettings::~UserSettings() = default;

//--------------------------------------------------------------------------------------------------
QString UserSettings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::lastDatabase() const
{
    return settings_.value(kLastDatabaseParam).toString();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setLastDatabase(const QString& id)
{
    settings_.setValue(kLastDatabaseParam, id);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isConfirmCloseEnabled() const
{
    return settings_.value(kConfirmCloseParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setConfirmCloseEnabled(bool enable)
{
    settings_.setValue(kConfirmCloseParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isCheckUpdatesEnabled() const
{
    return settings_.value(kCheckUpdatesParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setCheckUpdatesEnabled(bool enable)
{
    settings_.setValue(kCheckUpdatesParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString UserSettings::updateChannel() const
{
    QString channel = settings_.value(kUpdateChannelParam).toString();
    if (channel.isEmpty())
        channel = kStableUpdateChannel;
    return channel;
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setUpdateChannel(const QString& channel)
{
    settings_.setValue(kUpdateChannelParam, channel);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpEnabled() const
{
    return settings_.value(kMcpEnabledParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpEnabled(bool enable)
{
    settings_.setValue(kMcpEnabledParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpAutoConfirmEnabled() const
{
    return settings_.value(kMcpAutoConfirmParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpAutoConfirmEnabled(bool enable)
{
    settings_.setValue(kMcpAutoConfirmParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool UserSettings::isMcpSignInEnabled() const
{
    return settings_.value(kMcpSignInParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpSignInEnabled(bool enable)
{
    settings_.setValue(kMcpSignInParam, enable);
}

//--------------------------------------------------------------------------------------------------
quint32 UserSettings::mcpSessionTypes() const
{
    bool ok = false;
    const quint32 value = settings_.value(kMcpSessionTypesParam).toUInt(&ok);
    if (!ok)
        return proto::peer::SESSION_TYPE_ALL;

    return value;
}

//--------------------------------------------------------------------------------------------------
void UserSettings::setMcpSessionTypes(quint32 session_types)
{
    settings_.setValue(kMcpSessionTypesParam, session_types);
}
