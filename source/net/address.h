//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CONSOLE__ADDRESS_H
#define CONSOLE__ADDRESS_H

#include <QString>

#include "build/build_config.h"

namespace net {

class Address
{
public:
    Address() = default;

    Address(const Address& other);
    Address& operator=(const Address& other);

    Address(Address&& other);
    Address& operator=(Address&& other);

    ~Address() = default;

    static Address fromString(const QString& str);
    static Address fromStdString(const std::string& str);

    QString toString() const;
    std::string toStdString() const;

    void setHost(const QString& host);
    QString host() const;

    void setPort(uint16_t port);
    uint16_t port() const;

    bool isValid() const;

    bool isEqual(const Address& other);

    bool operator==(const Address& other);
    bool operator!=(const Address& other);

private:
    Address(QString&& host, uint16_t port);

    QString host_;
    uint16_t port_ = 0;
};

} // namespace net

#endif // CONSOLE__ADDRESS_H
