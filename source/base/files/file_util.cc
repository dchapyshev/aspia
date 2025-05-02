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

#include "base/files/file_util.h"

#include <QFile>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <class Container>
bool readFileT(const QString& filename, Container* buffer)
{
    if (!buffer)
        return false;

    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
        return false;

    qint64 size = file.size();
    buffer->clear();

    if (size <= 0)
        return true;

    buffer->resize(static_cast<Container::size_type>(size));

    return file.read(reinterpret_cast<char*>(buffer->data()), buffer->size()) != -1;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool writeFile(const QString& filename, const void* data, size_t size)
{
    if (!data)
        return false;

    QFile file(filename);
    if (!file.open(QFile::ReadWrite | QFile::Truncate))
        return false;

    return file.write(reinterpret_cast<const char*>(data), size) != -1;
}

//--------------------------------------------------------------------------------------------------
bool writeFile(const QString& filename, const QByteArray& buffer)
{
    return writeFile(filename, buffer.data(), buffer.size());
}

//--------------------------------------------------------------------------------------------------
bool writeFile(const QString& filename, std::string_view buffer)
{
    return writeFile(filename, buffer.data(), buffer.size());
}

//--------------------------------------------------------------------------------------------------
bool readFile(const QString& filename, QByteArray* buffer)
{
    return readFileT<QByteArray>(filename, buffer);
}

//--------------------------------------------------------------------------------------------------
bool readFile(const QString& filename, std::string* buffer)
{
    return readFileT<std::string>(filename, buffer);
}

} // namespace base
