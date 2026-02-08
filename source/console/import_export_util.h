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

#ifndef CONSOLE_IMPORT_EXPORT_UTIL_H
#define CONSOLE_IMPORT_EXPORT_UTIL_H

#include <QJsonDocument>

#include "proto/address_book.h"

namespace console {

void importComputersFromJson(
    const QJsonDocument& json, proto::address_book::ComputerGroup* parent_computer_group);

QJsonDocument exportComputersToJson(const proto::address_book::ComputerGroup& computer_group);

} // namespace console

#endif // CONSOLE_IMPORT_EXPORT_UTIL_H
