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

#ifndef BASE_NET_NET_UTILS_H
#define BASE_NET_NET_UTILS_H

#include <QStringList>

namespace base {

class NetUtils
{
public:
    static QStringList localIpList();
    static bool isValidIpAddress(const QString& ip_address);
    static bool isValidHostName(const QString& host);
    static bool isValidPort(quint16 port);
    static bool isValidPort(const QString& str);
    static bool isValidSubnet(const QString& subnet);

private:
    Q_DISABLE_COPY_MOVE(NetUtils)
};

} // namespace base

#endif // BASE_NET_NET_UTILS_H
