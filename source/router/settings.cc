//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
    setMinLogLevel(1);
    setClientWhiteList(std::vector<std::u16string>());
    setHostWhiteList(std::vector<std::u16string>());
    setAdminWhiteList(std::vector<std::u16string>());
    setRelayWhiteList(std::vector<std::u16string>());
}

void Settings::flush()
{
    impl_.flush();
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

void Settings::setMinLogLevel(int level)
{
    impl_.set<int>("MinLogLevel", level);
}

int Settings::minLogLevel() const
{
    return impl_.get<int>("MinLogLevel", 1);
}

void Settings::setClientWhiteList(const std::vector<std::u16string>& list)
{
    std::u16string result;

    for (const auto& entry : list)
        base::strAppend(&result, { entry, u";" });

    impl_.set<std::u16string>("ClientWhiteList", result);
}

std::vector<std::u16string> Settings::clientWhiteList() const
{
    return base::splitString(impl_.get<std::u16string>("ClientWhiteList"),
                             u";",
                             base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
}

void Settings::setHostWhiteList(const std::vector<std::u16string>& list)
{
    std::u16string result;

    for (const auto& entry : list)
        base::strAppend(&result, { entry, u";" });

    impl_.set<std::u16string>("HostWhiteList", result);
}

std::vector<std::u16string> Settings::hostWhiteList() const
{
    return base::splitString(impl_.get<std::u16string>("HostWhiteList"),
                             u";",
                             base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
}

void Settings::setAdminWhiteList(const std::vector<std::u16string>& list)
{
    std::u16string result;

    for (const auto& entry : list)
        base::strAppend(&result, { entry, u";" });

    impl_.set<std::u16string>("AdminWhiteList", result);
}

std::vector<std::u16string> Settings::adminWhiteList() const
{
    return base::splitString(impl_.get<std::u16string>("AdminWhiteList"),
                             u";",
                             base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
}

void Settings::setRelayWhiteList(const std::vector<std::u16string>& list)
{
    std::u16string result;

    for (const auto& entry : list)
        base::strAppend(&result, { entry, u";" });

    impl_.set<std::u16string>("RelayWhiteList", result);
}

std::vector<std::u16string> Settings::relayWhiteList() const
{
    return base::splitString(impl_.get<std::u16string>("RelayWhiteList"),
                             u";",
                             base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
}

} // namespace router
