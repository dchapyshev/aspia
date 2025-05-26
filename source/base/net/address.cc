//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/address.h"

#include <QHostAddress>

namespace base {

namespace {

const int kMaxHostNameLength = 64;

//--------------------------------------------------------------------------------------------------
bool isValidHostNameChar(const QChar c)
{
    if (c.isLetterOrNumber())
        return true;

    if (c == '.' || c == '_' || c == '-')
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
bool isValidHostName(const QString& host)
{
    if (host.isEmpty())
        return false;

    int length = host.length();
    if (length > kMaxHostNameLength)
        return false;

    size_t letter_count = 0;
    size_t digit_count = 0;

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

//--------------------------------------------------------------------------------------------------
bool isValidPort(quint16 port)
{
    return port != 0;
}

//--------------------------------------------------------------------------------------------------
bool isValidPort(const QString& str)
{
    bool ok = false;
    quint16 value = str.toUShort(&ok);
    if (!ok)
        return false;

    return isValidPort(value);
};

struct AddressParts
{
    QString host;
    QString port;
};

//--------------------------------------------------------------------------------------------------
QString fromIterators(QString::const_iterator start, QString::const_iterator end)
{
    return QString(start, end - start);
}

//--------------------------------------------------------------------------------------------------
bool setHostAndPort(QString::const_iterator first,
                    QString::const_iterator last,
                    QString::const_iterator last_colon,
                    AddressParts* parts)
{
    if (first >= last_colon)
    {
        parts->host = fromIterators(first, last);
    }
    else
    {
        auto port_start = last_colon;
        ++port_start;

        parts->host = fromIterators(first, last_colon);
        parts->port = fromIterators(port_start, last);

        if (!isValidPort(parts->port))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parse(QString::const_iterator& it, QString::const_iterator last, AddressParts* parts)
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
            if (*first == ':')
                return false;

            if (*first == '[')
            {
                state = ParseState::HOST_IPV6;
                first = it;
                continue;
            }

            if (*it == ':')
            {
                parts->host = fromIterators(first, it);
                state = ParseState::PORT;
                ++it;
                first = it;
                continue;
            }
        }
        else if (state == ParseState::HOST_IPV6)
        {
            if (*first != '[')
                return false;

            if (*it == ']')
            {
                ++it;

                if (it == last)
                {
                    break;
                }
                else if (*it == ':')
                {
                    parts->host = fromIterators(first + 1, it - 1);
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
        parts->port = fromIterators(first, last);
        if (!isValidPort(parts->port))
            return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Address::Address(quint16 default_port)
    : default_port_(default_port)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Address::Address(QString&& host, quint16 port, quint16 default_port)
    : host_(std::move(host)),
      port_(port),
      default_port_(default_port)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Address::Address(const Address& other)
    : host_(other.host_),
      port_(other.port_),
      default_port_(other.default_port_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Address& Address::operator=(const Address& other)
{
    if (this != &other)
    {
        host_ = other.host_;
        port_ = other.port_;
        default_port_ = other.default_port_;
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
Address::Address(Address&& other) noexcept
    : host_(std::move(other.host_)),
      port_(other.port_),
      default_port_(other.default_port_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Address& Address::operator=(Address&& other) noexcept
{
    if (this != &other)
    {
        host_ = std::move(other.host_);
        port_ = other.port_;
        default_port_ = other.default_port_;
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
// static
Address Address::fromString(const QString& str, quint16 default_port)
{
    auto begin = str.cbegin();
    auto end = str.cend();

    AddressParts parts;
    if (parse(begin, end, &parts))
    {
        QHostAddress address(parts.host);

        if (address.protocol() == QAbstractSocket::IPv4Protocol ||
            address.protocol() == QAbstractSocket::IPv6Protocol ||
            isValidHostName(parts.host))
        {
            bool ok = false;
            quint16 port = parts.port.toUShort(&ok);

            if (!ok)
                port = default_port;

            return Address(std::move(parts.host), port, default_port);
        }
    }

    return Address(default_port);
}

//--------------------------------------------------------------------------------------------------
QString Address::toString() const
{
    if (!isValidPort(port_))
        return QString();

    QHostAddress address(host_);

    if (address.protocol() == QAbstractSocket::IPv6Protocol)
    {
        if (port_ == default_port_)
            return "[" + host_ + "]";
        else
            return "[" + host_ + "]:" + QString::number(port_);
    }
    else
    {
        if (address.protocol() != QAbstractSocket::IPv4Protocol && !isValidHostName(host_))
            return QString();

        if (port_ == default_port_)
            return host();

        return host_ + ":" + QString::number(port_);
    }
}

//--------------------------------------------------------------------------------------------------
void Address::setHost(const QString& host)
{
    host_ = host;
}

//--------------------------------------------------------------------------------------------------
QString Address::host() const
{
    return host_;
}

//--------------------------------------------------------------------------------------------------
void Address::setPort(quint16 port)
{
    port_ = port;
}

//--------------------------------------------------------------------------------------------------
quint16 Address::port() const
{
    return port_;
}

//--------------------------------------------------------------------------------------------------
bool Address::isValid() const
{
    if (!isValidPort(port_))
        return false;

    QHostAddress address(host_);

    return address.protocol() == QAbstractSocket::IPv4Protocol ||
           address.protocol() == QAbstractSocket::IPv6Protocol ||
           isValidHostName(host_);
}

//--------------------------------------------------------------------------------------------------
bool Address::isEqual(const Address& other)
{
    return host_ == other.host_ && port_ == other.port_;
}

//--------------------------------------------------------------------------------------------------
bool Address::operator==(const Address& other)
{
    return isEqual(other);
}

//--------------------------------------------------------------------------------------------------
bool Address::operator!=(const Address& other)
{
    return !isEqual(other);
}

} // namespace base
