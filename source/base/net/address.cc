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

#include "base/net/ip_util.h"
#include "base/strings/string_number_conversions.h"

#include <cwctype>

namespace base {

namespace {

const size_t kMaxHostNameLength = 64;

//--------------------------------------------------------------------------------------------------
bool isValidHostNameChar(const char16_t c)
{
    if (std::iswalnum(c))
        return true;

    if (c == '.' || c == '_' || c == '-')
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
bool isValidHostName(std::u16string_view host)
{
    if (host.empty())
        return false;

    size_t length = host.length();
    if (length > kMaxHostNameLength)
        return false;

    size_t letter_count = 0;
    size_t digit_count = 0;

    for (size_t i = 0; i < length; ++i)
    {
        if (std::iswdigit(host[i]))
            ++digit_count;

        if (std::iswalpha(host[i]))
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
bool isValidPort(std::u16string_view str)
{
    quint16 value;

    if (!stringToUShort(str, &value))
        return false;

    return isValidPort(value);
};

struct AddressParts
{
    std::u16string host;
    std::u16string port;
};

//--------------------------------------------------------------------------------------------------
bool setHostAndPort(std::u16string_view::const_iterator first,
                    std::u16string_view::const_iterator last,
                    std::u16string_view::const_iterator last_colon,
                    AddressParts* parts)
{
    if (first >= last_colon)
    {
        parts->host = std::u16string(first, last);
    }
    else
    {
        auto port_start = last_colon;
        ++port_start;

        parts->host = std::u16string(first, last_colon);
        parts->port = std::u16string(port_start, last);

        if (!isValidPort(parts->port))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parse(std::u16string_view::const_iterator& it,
           std::u16string_view::const_iterator last,
           AddressParts* parts)
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
                parts->host = std::u16string(first, it);
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
                    parts->host = std::u16string(first + 1, it - 1);
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

            if (!std::iswdigit(*it))
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
        parts->port = std::u16string(first, last);
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
Address::Address(std::u16string&& host, quint16 port, quint16 default_port)
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
Address Address::fromString(std::u16string_view str, quint16 default_port)
{
    auto begin = str.cbegin();
    auto end = str.cend();

    AddressParts parts;

    if (parse(begin, end, &parts))
    {
        if (isValidIpV4Address(QString::fromStdU16String(parts.host)) ||
            isValidIpV6Address(QString::fromStdU16String(parts.host)) ||
            isValidHostName(parts.host))
        {
            quint16 port;

            if (!stringToUShort(parts.port, &port))
                port = default_port;

            return Address(std::move(parts.host), port, default_port);
        }
    }

    return Address(default_port);
}

//--------------------------------------------------------------------------------------------------
std::u16string Address::toString() const
{
    if (!isValidPort(port_))
        return std::u16string();

    if (isValidIpV6Address(QString::fromStdU16String(host_)))
    {
        if (port_ == default_port_)
        {
            return u"[" + host_ + u"]";
        }
        else
        {
            return u"[" + host_ + u"]:" + numberToString16(port_);
        }
    }
    else
    {
        if (!isValidIpV4Address(QString::fromStdU16String(host_)) && !isValidHostName(host_))
            return std::u16string();

        if (port_ == default_port_)
            return host();

        return host_ + u":" + numberToString16(port_);
    }
}

//--------------------------------------------------------------------------------------------------
void Address::setHost(std::u16string_view host)
{
    host_ = host;
}

//--------------------------------------------------------------------------------------------------
std::u16string Address::host() const
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

    QString host = QString::fromStdU16String(host_);

    if (!isValidIpV4Address(host) &&
        !isValidIpV6Address(host) &&
        !isValidHostName(host_))
    {
        return false;
    }

    return true;
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
