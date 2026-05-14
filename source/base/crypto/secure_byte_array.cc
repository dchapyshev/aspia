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

#include "base/crypto/secure_byte_array.h"

#include "base/crypto/secure_memory.h"

namespace {

void registerTypes()
{
    qRegisterMetaType<SecureByteArray>("SecureByteArray");
}

struct Registrator
{
    Registrator()
    {
        registerTypes();
    }
};

static volatile Registrator registrator;

} // namespace

//--------------------------------------------------------------------------------------------------
SecureByteArray::SecureByteArray(const QByteArray& data)
    : data_(data)
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureByteArray::SecureByteArray(QByteArray&& data) noexcept
    : data_(std::move(data))
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureByteArray::SecureByteArray(const char* data, qsizetype size)
    : data_(data, size)
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureByteArray::SecureByteArray(const SecureByteArray& other)
    : data_(other.data_.constData(), other.data_.size())
{
    data_.detach();
}

//--------------------------------------------------------------------------------------------------
SecureByteArray& SecureByteArray::operator=(const SecureByteArray& other)
{
    if (this != &other)
    {
        clear();
        data_ = QByteArray(other.data_.constData(), other.data_.size());
        data_.detach();
    }
    return *this;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray::SecureByteArray(SecureByteArray&& other) noexcept
    : data_(std::move(other.data_))
{
    other.data_ = QByteArray();
}

//--------------------------------------------------------------------------------------------------
SecureByteArray& SecureByteArray::operator=(SecureByteArray&& other) noexcept
{
    if (this != &other)
    {
        clear();
        data_ = std::move(other.data_);
        other.data_ = QByteArray();
    }
    return *this;
}

//--------------------------------------------------------------------------------------------------
SecureByteArray::~SecureByteArray()
{
    clear();
}

//--------------------------------------------------------------------------------------------------
void SecureByteArray::clear()
{
    if (!data_.isEmpty())
        memZero(&data_);
    data_.clear();
}
