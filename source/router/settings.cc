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

#include "router/settings.h"

#include "base/logging.h"
#include "base/crypto/secure_byte_array.h"
#include "base/files/base_paths.h"
#include "base/net/net_utils.h"
#include "build/build_config.h"

namespace {

using namespace Qt::StringLiterals;

// The literals hold their bytes statically, so naming a section costs no allocation.
const QByteArray kClientSection = "client"_ba;
const QByteArray kHostSection = "host"_ba;
const QByteArray kRelaySection = "relay"_ba;
const QByteArray kRouterSection = "router"_ba;
const QByteArray kStunSection = "stun"_ba;

const QByteArray kWhiteListKey = "white_list"_ba;

// A semicolon starts a comment in an ini file.
constexpr auto kWhiteListSeparator = ',';

//--------------------------------------------------------------------------------------------------
bool isValidWhiteListEntry(const QString& entry)
{
    if (NetUtils::isValidIpAddress(entry))
        return true;
    return NetUtils::isValidSubnet(entry);
}

//--------------------------------------------------------------------------------------------------
QString configFilePath()
{
    // The override lets tests and unusual deployments point the router at their own file.
    QString file_path = qEnvironmentVariable("ASPIA_ROUTER_CONFIG_FILE");
    if (!file_path.isEmpty())
        return file_path;

    return BasePaths::appConfigDir() + "/router.conf";
}

} // namespace
//--------------------------------------------------------------------------------------------------
Settings::Settings()
    // The file carries private keys, so no one but its owner may read it. Qt maps the permissions
    // to file modes only on posix; on windows the file is guarded by the ACL of its directory.
    : ini_(configFilePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings() = default;

//--------------------------------------------------------------------------------------------------
QString Settings::filePath()
{
    return ini_.filePath();
}

//--------------------------------------------------------------------------------------------------
bool Settings::isEmpty() const
{
    return ini_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool Settings::hasError() const
{
    return ini_.hasErrors();
}

//--------------------------------------------------------------------------------------------------
void Settings::reset()
{
    setListenInterface(QString());
    setHostPort(DEFAULT_ROUTER_HOST_TCP_PORT);
    setClientPort(DEFAULT_ROUTER_CLIENT_TCP_PORT);
    setRelayPort(DEFAULT_ROUTER_RELAY_TCP_PORT);
    setLegacyHostPort(DEFAULT_ROUTER_LEGACY_HOST_TCP_PORT);
    setHostPrivateKey(SecureByteArray());
    setRelayPrivateKey(SecureByteArray());
    setClientWhiteList(WhiteList());
    setHostWhiteList(WhiteList());
    setRelayWhiteList(WhiteList());
    setEnableStun(true);
    setStunPort(DEFAULT_STUN_PORT);
}

//--------------------------------------------------------------------------------------------------
bool Settings::sync()
{
    return ini_.sync();
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& iface)
{
    ini_.setStringValue(kRouterSection, "listen_interface", iface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return ini_.stringValue(kRouterSection, "listen_interface");
}

//--------------------------------------------------------------------------------------------------
void Settings::setLegacyHostPort(quint16 port)
{
    ini_.setUInt16Value(kHostSection, "legacy_port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::legacyHostPort() const
{
    return ini_.uint16Value(kHostSection, "legacy_port", DEFAULT_ROUTER_LEGACY_HOST_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setHostPort(quint16 port)
{
    ini_.setUInt16Value(kHostSection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::hostPort() const
{
    return ini_.uint16Value(kHostSection, "port", DEFAULT_ROUTER_HOST_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setClientPort(quint16 port)
{
    ini_.setUInt16Value(kClientSection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::clientPort() const
{
    return ini_.uint16Value(kClientSection, "port", DEFAULT_ROUTER_CLIENT_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setRelayPort(quint16 port)
{
    ini_.setUInt16Value(kRelaySection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::relayPort() const
{
    return ini_.uint16Value(kRelaySection, "port", DEFAULT_ROUTER_RELAY_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setHostPrivateKey(const SecureByteArray& private_key)
{
    ini_.setBinaryValue(kHostSection, "private_key", private_key.toByteArray());
}

//--------------------------------------------------------------------------------------------------
SecureByteArray Settings::hostPrivateKey() const
{
    return SecureByteArray(ini_.binaryValue(kHostSection, "private_key"));
}

//--------------------------------------------------------------------------------------------------
void Settings::setRelayPrivateKey(const SecureByteArray& private_key)
{
    ini_.setBinaryValue(kRelaySection, "private_key", private_key.toByteArray());
}

//--------------------------------------------------------------------------------------------------
SecureByteArray Settings::relayPrivateKey() const
{
    return SecureByteArray(ini_.binaryValue(kRelaySection, "private_key"));
}

//--------------------------------------------------------------------------------------------------
void Settings::setClientWhiteList(const WhiteList& list)
{
    setWhiteList(kClientSection, list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::clientWhiteList() const
{
    return whiteList(kClientSection);
}

//--------------------------------------------------------------------------------------------------
void Settings::setHostWhiteList(const WhiteList& list)
{
    setWhiteList(kHostSection, list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::hostWhiteList() const
{
    return whiteList(kHostSection);
}

//--------------------------------------------------------------------------------------------------
void Settings::setRelayWhiteList(const WhiteList& list)
{
    setWhiteList(kRelaySection, list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::relayWhiteList() const
{
    return whiteList(kRelaySection);
}

//--------------------------------------------------------------------------------------------------
void Settings::setSeedKey(const QByteArray &seed_key)
{
    ini_.setBinaryValue(kRouterSection, "seed_key", seed_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::seedKey() const
{
    return ini_.binaryValue(kRouterSection, "seed_key");
}

//--------------------------------------------------------------------------------------------------
void Settings::setEnableStun(bool enable)
{
    ini_.setBooleanValue(kStunSection, "enabled", enable);
}

//--------------------------------------------------------------------------------------------------
bool Settings::isStunEnabled() const
{
    return ini_.booleanValue(kStunSection, "enabled", true);
}

//--------------------------------------------------------------------------------------------------
void Settings::setStunPort(quint16 port)
{
    ini_.setUInt16Value(kStunSection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::stunPort() const
{
    return ini_.uint16Value(kStunSection, "port", DEFAULT_STUN_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setWhiteList(const QByteArray& section, const WhiteList& value)
{
    WhiteList result;

    for (const auto& entry : value)
    {
        const QString trimmed_entry = entry.trimmed();

        if (isValidWhiteListEntry(trimmed_entry))
        {
            result.append(trimmed_entry);
        }
        else
        {
            LOG(ERROR) << "Invalid IP address or subnet" << entry << "in section" << section;
        }
    }

    ini_.setStringValue(section, kWhiteListKey, result.join(kWhiteListSeparator));
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::whiteList(const QByteArray& section) const
{
    WhiteList result = ini_.stringValue(section, kWhiteListKey)
        .split(kWhiteListSeparator, Qt::SkipEmptyParts);

    auto it = result.begin();
    while (it != result.end())
    {
        *it = it->trimmed();

        if (isValidWhiteListEntry(*it))
        {
            ++it;
        }
        else
        {
            LOG(ERROR) << "Invalid IP address or subnet" << *it << "in section" << section;
            it = result.erase(it);
        }
    }

    return result;
}
