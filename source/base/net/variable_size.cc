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

#include "base/net/variable_size.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
asio::mutable_buffer VariableSizeReader::buffer()
{
    DCHECK_LT(pos_, std::size(buffer_));

    return asio::mutable_buffer(&buffer_[pos_], sizeof(quint8));
}

//--------------------------------------------------------------------------------------------------
std::optional<size_t> VariableSizeReader::messageSize()
{
    DCHECK_LT(pos_, std::size(buffer_));

    if (pos_ == 3 || !(buffer_[pos_] & 0x80))
    {
        size_t result = buffer_[0] & 0x7F;

        if (pos_ >= 1)
            result += (buffer_[1] & 0x7F) << 7;

        if (pos_ >= 2)
            result += (buffer_[2] & 0x7F) << 14;

        if (pos_ >= 3)
            result += buffer_[3] << 21;

        memset(buffer_, 0, std::size(buffer_));
        pos_ = 0;

        return result;
    }
    else
    {
        ++pos_;
        return std::nullopt;
    }
}

//--------------------------------------------------------------------------------------------------
asio::const_buffer VariableSizeWriter::variableSize(size_t size)
{
    size_t length = 1;

    buffer_[0] = size & 0x7F;
    if (size > 0x7F) // 127 bytes
    {
        buffer_[0] |= 0x80;
        buffer_[length++] = size >> 7 & 0x7F;

        if (size > 0x3FFF) // 16383 bytes
        {
            buffer_[1] |= 0x80;
            buffer_[length++] = size >> 14 & 0x7F;

            if (size > 0x1FFFF) // 2097151 bytes
            {
                buffer_[2] |= 0x80;
                buffer_[length++] = size >> 21 & 0xFF;
            }
        }
    }

    return asio::const_buffer(buffer_, length);
}

} // namespace base
