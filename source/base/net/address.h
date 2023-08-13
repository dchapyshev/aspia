//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_ADDRESS_H
#define BASE_NET_ADDRESS_H

#include "build/build_config.h"

#include <cstdint>
#include <string>

namespace base {

class Address
{
public:
    explicit Address(uint16_t default_port);

    Address(const Address& other);
    Address& operator=(const Address& other);

    Address(Address&& other) noexcept;
    Address& operator=(Address&& other) noexcept;

    ~Address() = default;

    static Address fromString(std::u16string_view str, uint16_t default_port);

    std::u16string toString() const;

    void setHost(std::u16string_view host);
    std::u16string host() const;

    void setPort(uint16_t port);
    uint16_t port() const;

    bool isValid() const;

    bool isEqual(const Address& other);

    bool operator==(const Address& other);
    bool operator!=(const Address& other);

private:
    Address(std::u16string&& host, uint16_t port, uint16_t default_port);

    std::u16string host_;
    uint16_t port_ = 0;
    uint16_t default_port_;
};

} // namespace base

#endif // BASE_NET_ADDRESS_H
