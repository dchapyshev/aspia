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

#include "base/logging.h"
#include "base/net/ip_util.h"

namespace router {

namespace {

const base::JsonSettings::Scope kScope = base::JsonSettings::Scope::SYSTEM;
const char kApplicationName[] = "aspia";
const char kFileName[] = "router";

} // namespace

//--------------------------------------------------------------------------------------------------
Settings::Settings()
    : impl_(kScope, kApplicationName, kFileName)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Settings::~Settings() = default;

//--------------------------------------------------------------------------------------------------
// static
std::filesystem::path Settings::filePath()
{
    return base::JsonSettings::filePath(kScope, kApplicationName, kFileName);
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
void Settings::flush()
{
    impl_.flush();
}

//--------------------------------------------------------------------------------------------------
void Settings::setListenInterface(const QString& interface)
{
    impl_.set<QString>("ListenInterface", interface);
}

//--------------------------------------------------------------------------------------------------
QString Settings::listenInterface() const
{
    return impl_.get<QString>("ListenInterface", QString());
}

//--------------------------------------------------------------------------------------------------
void Settings::setPort(quint16 port)
{
    impl_.set<quint16>("Port", port);
}

//--------------------------------------------------------------------------------------------------
quint16 Settings::port() const
{
    return impl_.get<quint16>("Port", DEFAULT_ROUTER_TCP_PORT);
}

//--------------------------------------------------------------------------------------------------
void Settings::setPrivateKey(const QByteArray& private_key)
{
    impl_.set<std::string>("PrivateKey", private_key.toHex().toStdString());
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::privateKey() const
{
    return QByteArray::fromHex(QByteArray::fromStdString(impl_.get<std::string>("PrivateKey")));
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
    impl_.set<std::string>("SeedKey", seed_key.toHex().toStdString());
}

//--------------------------------------------------------------------------------------------------
QByteArray Settings::seedKey() const
{
    return QByteArray::fromHex(QByteArray::fromStdString(impl_.get<std::string>("SeedKey")));
}

//--------------------------------------------------------------------------------------------------
void Settings::setWhiteList(std::string_view key, const WhiteList& value)
{
    QString result;

    for (const auto& entry : value)
    {
        if (base::isValidIpV4Address(entry) || base::isValidIpV6Address(entry))
        {
            result += entry + ';';
        }
        else
        {
            LOG(LS_ERROR) << "Invalid IP address '" << entry << "' in " << key;
        }
    }

    impl_.set<QString>(key, result);
}

//--------------------------------------------------------------------------------------------------
Settings::WhiteList Settings::whiteList(std::string_view key) const
{
    WhiteList result = impl_.get<QString>(key).split(';', Qt::SkipEmptyParts);

    auto it = result.begin();
    while (it != result.end())
    {
        if (base::isValidIpV4Address(*it) || base::isValidIpV6Address(*it))
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
