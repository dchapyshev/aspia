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

#ifndef BASE_CRYPTO_SECURE_STRING_H
#define BASE_CRYPTO_SECURE_STRING_H

#include <QString>

#include "base/crypto/secure_byte_array.h"

class SecureString final
{
public:
    SecureString() = default;
    explicit SecureString(const QString& string);
    explicit SecureString(QString&& string) noexcept;

    SecureString(const SecureString& other);
    SecureString& operator=(const SecureString& other);

    SecureString(SecureString&& other) noexcept;
    SecureString& operator=(SecureString&& other) noexcept;

    ~SecureString();

    bool isEmpty() const { return data_.isEmpty(); }
    qsizetype size() const { return data_.size(); }

    const QString& toString() const { return data_; }
    SecureByteArray toUtf8() const { return SecureByteArray(data_.toUtf8()); }

    static SecureString fromUtf8(const SecureByteArray& utf8);

    void clear();

    bool operator==(const SecureString& other) const { return data_ == other.data_; }
    bool operator!=(const SecureString& other) const { return data_ != other.data_; }

private:
    QString data_;
};

Q_DECLARE_METATYPE(SecureString)

#endif // BASE_CRYPTO_SECURE_STRING_H
