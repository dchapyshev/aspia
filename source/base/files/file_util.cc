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

#include <fstream>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
template <class Container>
bool readFileT(const std::filesystem::path& filename, Container* buffer)
{
    if (!buffer)
        return false;

    std::ifstream stream;
    stream.open(filename, std::ifstream::binary | std::ifstream::in);
    if (!stream.is_open())
        return false;

    stream.seekg(0, stream.end);
    size_t size = static_cast<size_t>(stream.tellg());
    stream.seekg(0);

    buffer->clear();

    if (!size)
        return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (size >= buffer->max_size())
        return false;
#endif

    buffer->resize(size);

    stream.read(reinterpret_cast<char*>(buffer->data()), buffer->size());
    return !stream.fail();
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool writeFile(const std::filesystem::path& filename, const void* data, size_t size)
{
    if (!data)
        return false;

    std::ofstream stream;
    stream.open(filename, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
    if (!stream.is_open())
        return false;

    stream.seekp(0);
    stream.write(reinterpret_cast<const char*>(data), size);

    return !stream.fail();
}

//--------------------------------------------------------------------------------------------------
bool writeFile(const std::filesystem::path& filename, const QByteArray& buffer)
{
    return writeFile(filename, buffer.data(), buffer.size());
}

//--------------------------------------------------------------------------------------------------
bool writeFile(const std::filesystem::path& filename, std::string_view buffer)
{
    return writeFile(filename, buffer.data(), buffer.size());
}

//--------------------------------------------------------------------------------------------------
bool readFile(const std::filesystem::path& filename, QByteArray* buffer)
{
    return readFileT<QByteArray>(filename, buffer);
}

//--------------------------------------------------------------------------------------------------
bool readFile(const std::filesystem::path& filename, std::string* buffer)
{
    return readFileT<std::string>(filename, buffer);
}

} // namespace base
