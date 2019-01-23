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

#include "console/computer_address.h"

#include <QStringView>

#include "net/ip_util.h"

namespace console {

namespace {

const int kMaxHostNameLength = 64;

bool isValidHostNameChar(const QChar& c)
{
    if (c.isLetterOrNumber())
        return true;

    if (c == QLatin1Char('.') || c == QLatin1Char('_') || c == QLatin1Char('-'))
        return true;

    return false;
}

bool isValidHostName(const QString& host)
{
    if (host.isEmpty())
        return false;

    int length = host.length();
    if (length > kMaxHostNameLength)
        return false;

    for (int i = 0; i < length; ++i)
    {
        if (!isValidHostNameChar(host[i]))
            return false;
    }

    return true;
}

bool isValidPort(uint16_t port)
{
    return (port != 0 && port < std::numeric_limits<uint16_t>::max());
}

bool isValidPort(const QString& str)
{
    bool ok = false;

    uint16_t value = str.toUShort(&ok);
    if (!ok)
        return false;

    return isValidPort(value);
};

struct AddressParts
{
    QString host;
    QString port;
};

bool setHostAndPort(QStringView::const_iterator first,
                    QStringView::const_iterator last,
                    QStringView::const_iterator last_colon,
                    AddressParts* parts)
{
    if (first >= last_colon)
    {
        parts->host = QStringView(first, last).toString();
    }
    else
    {
        auto port_start = last_colon;
        ++port_start;

        parts->host = QStringView(first, last_colon).toString();
        parts->port = QStringView(port_start, last).toString();

        if (!isValidPort(parts->port))
            return false;
    }

    return true;
}

bool parse(QStringView::const_iterator& it, QStringView::const_iterator last, AddressParts* parts)
{
    enum class ParseState { HOST, HOST_IPV6, PORT };

    if (it == last)
        return false;

    auto state = ParseState::HOST;
    auto first = it;
    auto last_colon = first;

    while (it != last)
    {
        if (state == ParseState::HOST)
        {
            if (*first == QLatin1Char(':'))
                return false;

            if (*first == QLatin1Char('['))
            {
                state = ParseState::HOST_IPV6;
                first = it;
                continue;
            }

            if (*it == QLatin1Char(':'))
            {
                parts->host = QStringView(first, it).toString();
                state = ParseState::PORT;
                ++it;
                first = it;
                continue;
            }
        }
        else if (state == ParseState::HOST_IPV6)
        {
            if (*first != QLatin1Char('['))
                return false;

            if (*it == QLatin1Char(']'))
            {
                ++it;

                if (it == last)
                {
                    break;
                }
                else if (*it == QLatin1Char(':'))
                {
                    parts->host = QStringView(first + 1, it - 1).toString();
                    state = ParseState::PORT;
                    ++it;
                    first = it;
                }

                continue;
            }
        }
        else if (state == ParseState::PORT)
        {
            if (it == last)
                return false;

            if (it->isDigit())
                ++it;

            continue;
        }

        ++it;
    }

    if (state == ParseState::HOST)
    {
        if (first == last)
            return false;

        if (!setHostAndPort(first, last, last_colon, parts))
            return false;
    }
    else if (state == ParseState::HOST_IPV6)
    {
        if (!setHostAndPort(first, last, last_colon, parts))
            return false;
    }
    else if (state == ParseState::PORT)
    {
        parts->port = QStringView(first, last).toString();
        if (!isValidPort(parts->port))
            return false;
    }

    return true;
}

} // namespace

ComputerAddress::ComputerAddress(QString&& host, uint16_t port)
    : host_(std::move(host)),
      port_(port)
{
    // Nothing
}

ComputerAddress::ComputerAddress(const ComputerAddress& other)
    : host_(other.host_),
      port_(other.port_)
{
    // Nothing
}

ComputerAddress& ComputerAddress::operator=(const ComputerAddress& other)
{
    if (this != &other)
    {
        host_ = other.host_;
        port_ = other.port_;
    }

    return *this;
}

ComputerAddress::ComputerAddress(ComputerAddress&& other)
    : host_(std::move(other.host_)),
      port_(other.port_)
{
    // Nothing
}

ComputerAddress& ComputerAddress::operator=(ComputerAddress&& other)
{
    if (this != &other)
    {
        host_ = std::move(other.host_);
        port_ = other.port_;
    }

    return *this;
}

// static
ComputerAddress ComputerAddress::fromString(const QString& str)
{
    auto begin = str.cbegin();
    auto end = str.cend();

    AddressParts parts;

    if (parse(begin, end, &parts))
    {
        if (net::isValidIpV4Address(parts.host) || net::isValidIpV6Address(parts.host) ||
            isValidHostName(parts.host))
        {
            uint16_t port = parts.port.toUShort();
            if (!isValidPort(port))
                port = DEFAULT_HOST_TCP_PORT;

            return ComputerAddress(std::move(parts.host), port);
        }
    }

    return ComputerAddress();
}

// static
ComputerAddress ComputerAddress::fromStdString(const std::string& str)
{
    return fromString(QString::fromStdString(str));
}

QString ComputerAddress::toString() const
{
    if (port_ == DEFAULT_HOST_TCP_PORT)
        return host();

    if (!isValidPort(port_) || !isValidHostName(host_))
        return QString();

    if (net::isValidIpV6Address(host_))
        return QString("[%1]:%2").arg(host_).arg(port_);

    return QString("%1:%2").arg(host_).arg(port_);
}

std::string ComputerAddress::toStdString() const
{
    return toString().toStdString();
}

void ComputerAddress::setHost(const QString& host)
{
    host_ = host;
}

QString ComputerAddress::host() const
{
    return host_;
}

void ComputerAddress::setPort(uint16_t port)
{
    port_ = port;
}

uint16_t ComputerAddress::port() const
{
    return port_;
}

bool ComputerAddress::isValid() const
{
    return isValidHostName(host_) && isValidPort(port_);
}

bool ComputerAddress::isEqual(const ComputerAddress& other)
{
    return host_ == other.host_ && port_ == other.port_;
}

bool ComputerAddress::operator==(const ComputerAddress& other)
{
    return isEqual(other);
}

bool ComputerAddress::operator!=(const ComputerAddress& other)
{
    return !isEqual(other);
}

} // namespace console
