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

#include "base/files/base_paths.h"
#include "build/build_config.h"

namespace router {

Settings::Settings()
    : impl_("aspia_router")
{
    // Nothing
}

Settings::~Settings() = default;

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

void Settings::setLogPath(const std::filesystem::path& path)
{
    impl_.set<std::filesystem::path>("LogPath", path);
}

std::filesystem::path Settings::logPath() const
{
    std::filesystem::path path = impl_.get<std::filesystem::path>("LogPath");
    if (path.empty())
    {
#if defined(OS_WIN)
        if (!base::BasePaths::commonAppData(&path))
            return std::filesystem::path();
        path.append("Aspia/Logs");
#else
#error Not implemented
#endif
    }

    return path;
}

void Settings::setMinLogLevel(int level)
{
    impl_.set<int>("MinLogLevel", level);
}

int Settings::minLogLevel() const
{
    return impl_.get<int>("MinLogLevel", 1);
}

void Settings::setMaxLogAge(int age)
{
    impl_.set<int>("MaxLogAge", age);
}

int Settings::maxLogAge() const
{
    return impl_.get<int>("MaxLogAge", 7);
}

} // namespace router
