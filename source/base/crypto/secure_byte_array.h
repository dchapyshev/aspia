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

#ifndef BASE_CRYPTO_SECURE_BYTE_ARRAY_H
#define BASE_CRYPTO_SECURE_BYTE_ARRAY_H

#include <QByteArray>
#include <QMetaType>

class SecureByteArray final
{
public:
    SecureByteArray() = default;
    explicit SecureByteArray(const QByteArray& data);
    explicit SecureByteArray(QByteArray&& data) noexcept;
    SecureByteArray(const char* data, qsizetype size);

    SecureByteArray(const SecureByteArray& other);
    SecureByteArray& operator=(const SecureByteArray& other);

    SecureByteArray(SecureByteArray&& other) noexcept;
    SecureByteArray& operator=(SecureByteArray&& other) noexcept;

    ~SecureByteArray();

    bool isEmpty() const { return data_.isEmpty(); }
    qsizetype size() const { return data_.size(); }

    const char* constData() const { return data_.constData(); }
    const QByteArray& toByteArray() const { return data_; }

    void clear();

    bool operator==(const SecureByteArray& other) const { return data_ == other.data_; }
    bool operator!=(const SecureByteArray& other) const { return data_ != other.data_; }

private:
    QByteArray data_;
};

Q_DECLARE_METATYPE(SecureByteArray)

#endif // BASE_CRYPTO_SECURE_BYTE_ARRAY_H
