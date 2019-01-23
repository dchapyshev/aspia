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

#ifndef CONSOLE__COMPUTER_ADDRESS_H
#define CONSOLE__COMPUTER_ADDRESS_H

#include <QString>

namespace console {

class ComputerAddress
{
public:
    static const uint16_t kInvalidPort = 0;

    ComputerAddress() = default;

    ComputerAddress(const ComputerAddress& other);
    ComputerAddress& operator=(const ComputerAddress& other);

    ComputerAddress(ComputerAddress&& other);
    ComputerAddress& operator=(ComputerAddress&& other);

    ~ComputerAddress() = default;

    static ComputerAddress fromString(const QString& str);
    static ComputerAddress fromStdString(const std::string& str);

    QString toString(uint16_t def_port = kInvalidPort) const;
    std::string toStdString() const;

    void setHost(const QString& host);
    QString host() const;

    void setPort(uint16_t port);
    uint16_t port(uint16_t def_port) const;

    bool isValid() const;

    bool isEqual(const ComputerAddress& other);

    bool operator==(const ComputerAddress& other);
    bool operator!=(const ComputerAddress& other);

private:
    ComputerAddress(QString&& host, uint16_t port);

    QString host_;
    uint16_t port_ = kInvalidPort;
};

} // namespace console

#endif // CONSOLE__COMPUTER_ADDRESS_H
