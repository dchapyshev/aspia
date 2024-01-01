//
// Aspia Project
// Copyright (C) 2016-2024 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_PRINTER_ENUMERATOR_H
#define BASE_WIN_PRINTER_ENUMERATOR_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"

#include <memory>
#include <string>

struct _PRINTER_INFO_2W;

namespace base::win {

class PrinterEnumerator
{
public:
    PrinterEnumerator();
    ~PrinterEnumerator();

    bool isAtEnd() const;
    void advance();

    bool isDefault() const;
    bool isShared() const;
    std::string name() const;
    std::string shareName() const;
    std::string portName() const;
    std::string driverName() const;
    std::string deviceName() const;
    int paperWidth() const;
    int paperHeight() const;
    int printQuality() const;
    std::string printProcessor() const;
    std::string dataType() const;
    std::string serverName() const;
    std::string location() const;
    std::string comment() const;
    int jobsCount() const;

private:
    ByteArray info_buffer_;
    std::wstring default_printer_;

    _PRINTER_INFO_2W* info_ = nullptr;
    int count_ = 0;
    int current_ = 0;

    DISALLOW_COPY_AND_ASSIGN(PrinterEnumerator);
};

} // namespace base::win

#endif // BASE_WIN_PRINTER_ENUMERATOR_H
