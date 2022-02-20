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

#ifndef ROUTER_SETTINGS_H
#define ROUTER_SETTINGS_H

#include "base/settings/json_settings.h"

namespace router {

class Settings
{
public:
    Settings();
    ~Settings();

    static std::filesystem::path filePath();

    void reset();
    void flush();

    void setPort(uint16_t port);
    uint16_t port() const;

    void setPrivateKey(const base::ByteArray& private_key);
    base::ByteArray privateKey() const;

    using WhiteList = std::vector<std::u16string>;

    void setClientWhiteList(const WhiteList& list);
    WhiteList clientWhiteList() const;

    void setHostWhiteList(const WhiteList& list);
    WhiteList hostWhiteList() const;

    void setAdminWhiteList(const WhiteList& list);
    WhiteList adminWhiteList() const;

    void setRelayWhiteList(const WhiteList& list);
    WhiteList relayWhiteList() const;

private:
    void setWhiteList(std::string_view key, const WhiteList& value);
    WhiteList whiteList(std::string_view key) const;

    base::JsonSettings impl_;
};

} // namespace router

#endif // ROUTER_SETTINGS_H
