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

#include "client/ui/client_settings.h"

#include "base/crypto/data_cryptor.h"
#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/build_config.h"
#include "client/config_factory.h"

#include <QLocale>

namespace client {

namespace {

const QString kLocaleParam = "Locale";
const QString kThemeParam = "Theme";
const QString kSessionTypeParam = "SessionType";
const QString kDesktopConfigParam = "DesktopConfig";
const QString kCheckUpdatesParam = "CheckUpdates";
const QString kUpdateServerParam = "UpdateServer";
const QString kOneTimePasswordCheckedParam = "OneTimePasswordChecked";
const QString kRouterManagerStateParam = "RouterManagerState";
const QString kDisplayNameParam = "DisplayName";
const QString kRouterEnabledParam = "RouterEnabled";
const QString kRouterAddressParam = "RouterAddress";
const QString kRouterPortParam = "RouterPort";
const QString kRouterUsernameParam = "RouterUsername";
const QString kRouterPasswordParam = "RouterPassword";

} // namespace

//--------------------------------------------------------------------------------------------------
ClientSettings::ClientSettings()
    : settings_(base::XmlSettings::format(), QSettings::UserScope, "aspia", "client")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::locale() const
{
    return settings_.value(kLocaleParam, QLocale::system().bcp47Name()).toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setLocale(const QString& locale)
{
    settings_.setValue(kLocaleParam, locale);
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::theme() const
{
    return settings_.value(kThemeParam, "auto").toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setTheme(const QString& theme)
{
    settings_.setValue(kThemeParam, theme);
}

//--------------------------------------------------------------------------------------------------
proto::peer::SessionType ClientSettings::sessionType() const
{
    return static_cast<proto::peer::SessionType>(
        settings_.value(kSessionTypeParam, proto::peer::SESSION_TYPE_DESKTOP).toUInt());
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setSessionType(proto::peer::SessionType session_type)
{
    settings_.setValue(kSessionTypeParam, static_cast<quint32>(session_type));
}

//--------------------------------------------------------------------------------------------------
proto::control::Config ClientSettings::desktopConfig() const
{
    QByteArray buffer = settings_.value(kDesktopConfigParam).toByteArray();
    if (!buffer.isEmpty())
    {
        proto::control::Config config;
        if (config.ParseFromArray(buffer.data(), buffer.size()))
            return config;
    }

    return ConfigFactory::defaultDesktopConfig();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDesktopConfig(const proto::control::Config& config)
{
    QByteArray buffer;
    buffer.resize(static_cast<int>(config.ByteSizeLong()));

    config.SerializeWithCachedSizesToArray(reinterpret_cast<quint8*>(buffer.data()));
    settings_.setValue(kDesktopConfigParam, buffer);
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::checkUpdates() const
{
    return settings_.value(kCheckUpdatesParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setCheckUpdates(bool check)
{
    settings_.setValue(kCheckUpdatesParam, check);
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::updateServer() const
{
    return settings_.value(kUpdateServerParam, DEFAULT_UPDATE_SERVER).toString().toLower();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setUpdateServer(const QString& server)
{
    settings_.setValue(kUpdateServerParam, server.toLower());
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::isOneTimePasswordChecked() const
{
    return settings_.value(kOneTimePasswordCheckedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setOneTimePasswordChecked(bool check)
{
    settings_.setValue(kOneTimePasswordCheckedParam, check);
}

//--------------------------------------------------------------------------------------------------
QByteArray ClientSettings::routerManagerState() const
{
    return settings_.value(kRouterManagerStateParam, false).toByteArray();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setRouterManagerState(const QByteArray& state)
{
    settings_.setValue(kRouterManagerStateParam, state);
}

//--------------------------------------------------------------------------------------------------
QString ClientSettings::displayName() const
{
    return settings_.value(kDisplayNameParam).toString();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setDisplayName(const QString& display_name)
{
    settings_.setValue(kDisplayNameParam, display_name);
}

//--------------------------------------------------------------------------------------------------
bool ClientSettings::isRouterEnabled() const
{
    return settings_.value(kRouterEnabledParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setRouterEnabled(bool enabled)
{
    settings_.setValue(kRouterEnabledParam, enabled);
}

//--------------------------------------------------------------------------------------------------
RouterConfig ClientSettings::routerConfig() const
{
    RouterConfig config;
    config.port = static_cast<quint16>(settings_.value(kRouterPortParam, 0).toUInt());

    base::DataCryptor& cryptor = base::DataCryptor::instance();
    QByteArray out;

    if (cryptor.decrypt(settings_.value(kRouterAddressParam).toByteArray(), &out))
        config.address = QString::fromUtf8(out);
    else
        LOG(ERROR) << "Failed to decrypt router address";

    if (cryptor.decrypt(settings_.value(kRouterUsernameParam).toByteArray(), &out))
        config.username = QString::fromUtf8(out);
    else
        LOG(ERROR) << "Failed to decrypt router username";

    if (cryptor.decrypt(settings_.value(kRouterPasswordParam).toByteArray(), &out))
        config.password = QString::fromUtf8(out);
    else
        LOG(ERROR) << "Failed to decrypt router password";

    return config;
}

//--------------------------------------------------------------------------------------------------
void ClientSettings::setRouterConfig(const RouterConfig& config)
{
    settings_.setValue(kRouterPortParam, config.port);

    base::DataCryptor& cryptor = base::DataCryptor::instance();
    QByteArray out;

    if (cryptor.encrypt(config.address.toUtf8(), &out))
        settings_.setValue(kRouterAddressParam, out);
    else
        LOG(ERROR) << "Failed to encrypt router address";

    if (cryptor.encrypt(config.username.toUtf8(), &out))
        settings_.setValue(kRouterUsernameParam, out);
    else
        LOG(ERROR) << "Failed to encrypt router username";

    if (cryptor.encrypt(config.password.toUtf8(), &out))
        settings_.setValue(kRouterPasswordParam, out);
    else
        LOG(ERROR) << "Failed to encrypt router password";
}

} // namespace client
