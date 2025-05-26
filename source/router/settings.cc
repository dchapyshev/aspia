//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QHostAddress>

#include "base/logging.h"
#include "base/xml_settings.h"
#include "build/build_config.h"

namespace router {

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : impl_(base::XmlSettings::format(), QSettings::SystemScope, "aspia", "router")
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool Settings::isEmpty() const
{
    return impl_.allKeys().isEmpty();
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings() = default;

//--------------------------------------------------------------------------------------------------
QString Settings::filePath()
{
    return impl_.fileName();
}

//--------------------------------------------------------------------------------------------------
void Settings::reset()
{
    setPort(DEFAULT_ROUTER_TCP_PORT);
    setPrivateKey(QByteArray());
    setClientWhiteList(WhiteList());
    setHostWhiteList(WhiteList());
    setAdminWhiteList(WhiteList());
    setRelayWhiteList(WhiteList());
}

//--------------------------------------------------------------------------------------------------
void Settings::sync()
{
    impl_.sync();
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& interface)
{
    impl_.setValue("ListenInterface", interface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return impl_.value("ListenInterface").toString();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPort(quint16 port)
{
    impl_.setValue("Port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::port() const
{
    return impl_.value("Port", DEFAULT_ROUTER_TCP_PORT).toUInt();
}

//--------------------------------------------------------------------------------------------------
void Settings::setPrivateKey(const QByteArray& private_key)
{
    impl_.setValue("PrivateKey", private_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::privateKey() const
{
    return impl_.value("PrivateKey").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setClientWhiteList(const WhiteList& list)
{
    setWhiteList("ClientWhiteList", list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::clientWhiteList() const
{
    return whiteList("ClientWhiteList");
}

//--------------------------------------------------------------------------------------------------
void Settings::setHostWhiteList(const WhiteList& list)
{
    setWhiteList("HostWhiteList", list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::hostWhiteList() const
{
    return whiteList("HostWhiteList");
}

//--------------------------------------------------------------------------------------------------
void Settings::setAdminWhiteList(const WhiteList& list)
{
    setWhiteList("AdminWhiteList", list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::adminWhiteList() const
{
    return whiteList("AdminWhiteList");
}

//--------------------------------------------------------------------------------------------------
void Settings::setRelayWhiteList(const WhiteList& list)
{
    setWhiteList("RelayWhiteList", list);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::relayWhiteList() const
{
    return whiteList("RelayWhiteList");
}

//--------------------------------------------------------------------------------------------------
void Settings::setSeedKey(const QByteArray &seed_key)
{
    impl_.setValue("SeedKey", seed_key);
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::seedKey() const
{
    return impl_.value("SeedKey").toByteArray();
}

//--------------------------------------------------------------------------------------------------
void Settings::setWhiteList(const QString& key, const WhiteList& value)
{
    QString result;

    for (const auto& entry : value)
    {
        QHostAddress address(entry);

        if (address.protocol() == QAbstractSocket::IPv4Protocol ||
            address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            result += entry + ';';
        }
        else
        {
            LOG(LS_ERROR) << "Invalid IP address '" << entry << "' in " << key;
        }
    }

    impl_.setValue(key, result);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::whiteList(const QString& key) const
{
    WhiteList result = impl_.value(key).toString().split(';', Qt::SkipEmptyParts);

    auto it = result.begin();
    while (it != result.end())
    {
        QHostAddress address(*it);

        if (address.protocol() == QAbstractSocket::IPv4Protocol ||
            address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            ++it;
        }
        else
        {
            LOG(LS_ERROR) << "Invalid IP address '" << *it << "' in " << key;
            it = result.erase(it);
        }
    }

    return result;
}

} // namespace router
