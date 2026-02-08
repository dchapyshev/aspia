//
// SmartCafe Project
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

#ifndef BASE_CRYPTO_GENERIC_HASH_H
#define BASE_CRYPTO_GENERIC_HASH_H

#include <QByteArray>

struct evp_md_ctx_st;
struct evp_md_st;

namespace base {

class GenericHash
{
public:
    enum Type
    {
        SHA1       = 1,
        BLAKE2b512 = 2,
        BLAKE2s256 = 3,
        SHA224     = 4,
        SHA256     = 5,
        SHA384     = 6,
        SHA512     = 7
    };

    explicit GenericHash(Type type);
    ~GenericHash();

    static QByteArray hash(Type type, const void* data, size_t size);
    static QByteArray hash(Type type, std::string_view data);
    static QByteArray hash(Type type, const QByteArray& data);

    void addData(const void* data, size_t size);
    void addData(std::string_view data);
    void addData(const QByteArray& data);

    QByteArray result() const;

    void reset();

private:
    evp_md_ctx_st* ctxt_ = nullptr;
    const evp_md_st* md_ = nullptr;

    Q_DISABLE_COPY(GenericHash)
};

} // namespace base

#endif // BASE_CRYPTO_GENERIC_HASH_H
