//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"

namespace router {

namespace {

const base::JsonSettings::Scope kScope = base::JsonSettings::Scope::SYSTEM;
const char kApplicationName[] = "aspia";
const char kFileName[] = "router";

} // namespace

Settings::Settings()
    : impl_(kScope, kApplicationName, kFileName)
{
    // Nothing
}

Settings::~Settings() = default;

// static
std::filesystem::path Settings::filePath()
{
    return base::JsonSettings::filePath(kScope, kApplicationName, kFileName);
}

void Settings::reset()
{
    setPort(DEFAULT_ROUTER_TCP_PORT);
    setPrivateKey(base::ByteArray());
    setClientWhiteList(WhiteList());
    setHostWhiteList(WhiteList());
    setAdminWhiteList(WhiteList());
    setRelayWhiteList(WhiteList());
}

void Settings::flush()
{
    impl_.flush();
}

void Settings::setListenInterface(const std::u16string& interface)
{
    impl_.set<std::u16string>("ListenInterface", interface);
}

std::u16string Settings::listenInterface() const
{
    std::u16string interface = impl_.get<std::u16string>("ListenInterface", u"0.0.0.0");
    if (interface.empty())
        return u"0.0.0.0";

    return interface;
}

void Settings::setPort(uint16_t port)
{
    impl_.set<uint16_t>("Port", port);
}

uint16_t Settings::port() const
{
    return impl_.get<uint16_t>("Port", DEFAULT_ROUTER_TCP_PORT);
}

void Settings::setPrivateKey(const base::ByteArray& private_key)
{
    impl_.set<std::string>("PrivateKey", base::toHex(private_key));
}

base::ByteArray Settings::privateKey() const
{
    return base::fromHex(impl_.get<std::string>("PrivateKey"));
}

void Settings::setClientWhiteList(const WhiteList& list)
{
    setWhiteList("ClientWhiteList", list);
}

Settings::WhiteList Settings::clientWhiteList() const
{
    return whiteList("ClientWhiteList");
}

void Settings::setHostWhiteList(const WhiteList& list)
{
    setWhiteList("HostWhiteList", list);
}

Settings::WhiteList Settings::hostWhiteList() const
{
    return whiteList("HostWhiteList");
}

void Settings::setAdminWhiteList(const WhiteList& list)
{
    setWhiteList("AdminWhiteList", list);
}

Settings::WhiteList Settings::adminWhiteList() const
{
    return whiteList("AdminWhiteList");
}

void Settings::setRelayWhiteList(const WhiteList& list)
{
    setWhiteList("RelayWhiteList", list);
}

Settings::WhiteList Settings::relayWhiteList() const
{
    return whiteList("RelayWhiteList");
}

void Settings::setWhiteList(std::string_view key, const WhiteList& value)
{
    std::u16string result;

    for (const auto& entry : value)
    {
        if (base::isValidIpV4Address(entry) || base::isValidIpV6Address(entry))
        {
            base::strAppend(&result, { entry, u";" });
        }
        else
        {
            LOG(LS_ERROR) << "Invalid IP address '" << entry << "' in " << key;
        }
    }

    impl_.set<std::u16string>(key, result);
}

Settings::WhiteList Settings::whiteList(std::string_view key) const
{
    WhiteList result =
        base::splitString(impl_.get<std::u16string>(key),
                          u";",
                          base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

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
