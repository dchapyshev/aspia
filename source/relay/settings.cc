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

#include "relay/settings.h"

#include "base/files/base_paths.h"
#include "build/build_config.h"

namespace {

using namespace Qt::StringLiterals;

// The literals hold their bytes statically, so naming a section costs no allocation.
const QByteArray kPeerSection = "peer"_ba;
const QByteArray kRouterSection = "router"_ba;

//--------------------------------------------------------------------------------------------------
QString configFilePath()
{
    // The override lets tests and unusual deployments point the relay at their own file.
    QString file_path = qEnvironmentVariable("ASPIA_RELAY_CONFIG_FILE");
    if (!file_path.isEmpty())
        return file_path;

    return BasePaths::appConfigDir() + "/relay.conf";
}

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    // Only the owner of the file may read and change it, as with the router configuration.
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
    setRouterAddress("127.0.0.1");
    setRouterPort(DEFAULT_ROUTER_RELAY_TCP_PORT);
    setRouterPublicKey(QByteArray());
    setListenInterface(QString());
    setPeerAddress(QString());
    setPeerPort(DEFAULT_RELAY_PEER_TCP_PORT);
    setPeerIdleTimeout(Minutes(5));
    setMaxPeerCount(100);
}

//--------------------------------------------------------------------------------------------------
bool Settings::sync()
{
    return ini_.sync();
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterAddress(const QString& address)
{
    ini_.setStringValue(kRouterSection, "address", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::routerAddress() const
{
    return ini_.stringValue(kRouterSection, "address");
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPort(quint16 port)
{
    ini_.setUInt16Value(kRouterSection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::routerPort() const
{
    return ini_.uint16Value(kRouterSection, "port", DEFAULT_ROUTER_RELAY_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setRouterPublicKey(const QByteArray& public_key)
{
    ini_.setBinaryValue(kRouterSection, "public_key", public_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::routerPublicKey() const
{
    return ini_.binaryValue(kRouterSection, "public_key");
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& iface)
{
    ini_.setStringValue(kPeerSection, "listen_interface", iface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return ini_.stringValue(kPeerSection, "listen_interface");
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerAddress(const QString& address)
{
    ini_.setStringValue(kPeerSection, "public_address", address);
}

//--------------------------------------------------------------------------------------------------
QString Settings::peerAddress() const
{
    return ini_.stringValue(kPeerSection, "public_address");
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerPort(quint16 port)
{
    ini_.setUInt16Value(kPeerSection, "port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::peerPort() const
{
    return ini_.uint16Value(kPeerSection, "port", DEFAULT_RELAY_PEER_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setPeerIdleTimeout(Minutes timeout)
{
    ini_.setInt64Value(kPeerSection, "idle_timeout", timeout.count());
}

//--------------------------------------------------------------------------------------------------
Minutes Settings::peerIdleTimeout() const
{
    return Minutes(ini_.int64Value(kPeerSection, "idle_timeout", 5));
}

//--------------------------------------------------------------------------------------------------
void Settings::setMaxPeerCount(quint32 count)
{
    ini_.setUInt32Value(kPeerSection, "max_count", count);
}

//--------------------------------------------------------------------------------------------------
quint32 Settings::maxPeerCount() const
{
    return ini_.uint32Value(kPeerSection, "max_count", 100);
}
