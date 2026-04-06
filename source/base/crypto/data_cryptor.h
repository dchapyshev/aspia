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

#ifndef BASE_CRYPTO_DATA_CRYPTOR_H
#define BASE_CRYPTO_DATA_CRYPTOR_H

#include <QByteArray>
#include <QByteArrayView>

namespace base {

class DataCryptor
{
public:
    DataCryptor();
    explicit DataCryptor(const QByteArray& key);
    ~DataCryptor();

    void setKey(const QByteArray& key);
    QByteArray key() const;
    bool hasKey() const;

    bool encrypt(QByteArrayView in, QByteArray* out);
    bool decrypt(QByteArrayView in, QByteArray* out);

    static DataCryptor& instance();

private:
    QByteArray key_;

    Q_DISABLE_COPY_MOVE(DataCryptor)
};

} // namespace base

#endif // BASE_CRYPTO_DATA_CRYPTOR_H
