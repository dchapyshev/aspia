//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CONST_BUFFER_H
#define BASE__CONST_BUFFER_H

#include <cstdint>

namespace base {

class ConstBuffer
{
public:
    ConstBuffer(const uint8_t* data, size_t size)
        : data_(data),
          size_(size)
    {
        // Nothing
    }

    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }

    bool isValid() const { return data_ != nullptr && size_ != 0; }

private:
    const uint8_t* data_;
    const size_t size_;
};

} // namespace base

#endif // BASE__CONST_BUFFER_H
