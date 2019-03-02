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

#include "net/address.h"

#include <QStringView>

#include "net/ip_util.h"

namespace net {

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

    int letter_count = 0;
    int digit_count = 0;

    for (int i = 0; i < length; ++i)
    {
        if (host[i].isDigit())
            ++digit_count;

        if (host[i].isLetter())
            ++letter_count;

        if (!isValidHostNameChar(host[i]))
            return false;
    }

    if (!letter_count && !digit_count)
        return false;

    return true;
}

bool isValidPort(uint16_t port)
{
    return port != 0;
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

            if (!it->isDigit())
                return false;
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
        if (!setHostAndPort(first + 1, last - 1, last_colon, parts))
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

Address::Address(QString&& host, uint16_t port)
    : host_(std::move(host)),
      port_(port)
{
    // Nothing
}

Address::Address(const Address& other)
    : host_(other.host_),
      port_(other.port_)
{
    // Nothing
}

Address& Address::operator=(const Address& other)
{
    if (this != &other)
    {
        host_ = other.host_;
        port_ = other.port_;
    }

    return *this;
}

Address::Address(Address&& other)
    : host_(std::move(other.host_)),
      port_(other.port_)
{
    // Nothing
}

Address& Address::operator=(Address&& other)
{
    if (this != &other)
    {
        host_ = std::move(other.host_);
        port_ = other.port_;
    }

    return *this;
}

// static
Address Address::fromString(const QString& str)
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

            return Address(std::move(parts.host), port);
        }
    }

    return Address();
}

// static
Address Address::fromStdString(const std::string& str)
{
    return fromString(QString::fromStdString(str));
}

QString Address::toString() const
{
    if (!isValidPort(port_))
        return QString();

    if (net::isValidIpV6Address(host_))
    {
        if (port_ == DEFAULT_HOST_TCP_PORT)
            return QString("[%1]").arg(host_);
        else
            return QString("[%1]:%2").arg(host_).arg(port_);
    }
    else
    {
        if (!net::isValidIpV4Address(host_) && !isValidHostName(host_))
            return QString();

        if (port_ == DEFAULT_HOST_TCP_PORT)
            return host();

        return QString("%1:%2").arg(host_).arg(port_);
    }
}

std::string Address::toStdString() const
{
    return toString().toStdString();
}

void Address::setHost(const QString& host)
{
    host_ = host;
}

QString Address::host() const
{
    return host_;
}

void Address::setPort(uint16_t port)
{
    port_ = port;
}

uint16_t Address::port() const
{
    return port_;
}

bool Address::isValid() const
{
    if (!isValidPort(port_))
        return false;

    if (!net::isValidIpV4Address(host_) &&
        !net::isValidIpV6Address(host_) &&
        !isValidHostName(host_))
    {
        return false;
    }

    return true;
}

bool Address::isEqual(const Address& other)
{
    return host_ == other.host_ && port_ == other.port_;
}

bool Address::operator==(const Address& other)
{
    return isEqual(other);
}

bool Address::operator!=(const Address& other)
{
    return !isEqual(other);
}

} // namespace net
