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

#include "base/meta_types.h"

#include <QMetaType>

#include <cstdint>
#include <filesystem>
#include <string>

namespace base {

void registerMetaTypes()
{
    qRegisterMetaType<std::filesystem::path>("std::filesystem::path");
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<std::u16string>("std::u16string");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<quint16>("quint16");
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<quint64>("quint64");
    qRegisterMetaType<int8_t>("int8_t");
    qRegisterMetaType<int16_t>("int16_t");
    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<size_t>("size_t");
}

} // namespace base
