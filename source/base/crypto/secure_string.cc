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

#include "base/crypto/secure_string.h"

#include "base/crypto/secure_memory.h"

//--------------------------------------------------------------------------------------------------
SecureString::SecureString(const QString& string)
    : data_(string)
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureString::SecureString(QString&& string) noexcept
    : data_(std::move(string))
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureString::SecureString(const SecureString& other)
    : data_(other.data_.constData(), other.data_.size())
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureString& SecureString::operator=(const SecureString& other)
{
    if (this != &other)
    {
        clear();
        data_ = QString(other.data_.constData(), other.data_.size());
        data_.detach();
    }
    return *this;
}

//--------------------------------------------------------------------------------------------------
SecureString::SecureString(SecureString&& other) noexcept
    : data_(std::move(other.data_))
{
    other.data_ = QString();
}

//--------------------------------------------------------------------------------------------------
SecureString& SecureString::operator=(SecureString&& other) noexcept
{
    if (this != &other)
    {
        clear();
        data_ = std::move(other.data_);
        other.data_ = QString();
    }
    return *this;
}

//--------------------------------------------------------------------------------------------------
SecureString::~SecureString()
{
    clear();
}

//--------------------------------------------------------------------------------------------------
void SecureString::clear()
{
    if (!data_.isEmpty())
        memZero(&data_);
    data_.clear();
}
