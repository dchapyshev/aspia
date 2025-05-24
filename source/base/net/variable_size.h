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

#ifndef BASE_NET_VARIABLE_SIZE_H
#define BASE_NET_VARIABLE_SIZE_H

#include <QtGlobal>

#include "base/macros_magic.h"

#include <asio/buffer.hpp>

#include <optional>

namespace base {

class VariableSizeReader
{
public:
    VariableSizeReader();
    ~VariableSizeReader();

    asio::mutable_buffer buffer();
    std::optional<size_t> messageSize();

private:
    quint8 buffer_[4] = { 0 };
    size_t pos_ = 0;

    DISALLOW_COPY_AND_ASSIGN(VariableSizeReader);
};

class VariableSizeWriter
{
public:
    VariableSizeWriter();
    ~VariableSizeWriter();

    asio::const_buffer variableSize(size_t size);

private:
    quint8 buffer_[4];

    DISALLOW_COPY_AND_ASSIGN(VariableSizeWriter);
};

} // namespace base

#endif // BASE_NET_VARIABLE_SIZE_H
