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

#include "base/win/printer_enumerator.h"
#include "base/logging.h"

#include <qt_windows.h>
#include <winspool.h>

namespace base {

//--------------------------------------------------------------------------------------------------
PrinterEnumerator::PrinterEnumerator()
{
    const DWORD flags = PRINTER_ENUM_FAVORITE | PRINTER_ENUM_LOCAL | PRINTER_ENUM_NETWORK;
    DWORD bytes_needed = 0;
    DWORD count = 0;

    if (EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes_needed, &count) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        PLOG(ERROR) << "Unexpected return value";
        return;
    }

    info_buffer_.resize(bytes_needed);

    if (!EnumPrintersW(flags, nullptr, 2, reinterpret_cast<LPBYTE>(info_buffer_.data()), bytes_needed,
                       &bytes_needed, &count))
    {
        PLOG(ERROR) << "EnumPrintersW failed";
        info_buffer_.clear();
        return;
    }

    wchar_t default_printer_name[256] = { 0 };
    DWORD characters_count = ARRAYSIZE(default_printer_name);

    if (!GetDefaultPrinterW(&default_printer_name[0], &characters_count))
    {
        PLOG(ERROR) << "GetDefaultPrinterW failed";
    }
    else
    {
        default_printer_ = default_printer_name;
    }

    info_ = reinterpret_cast<PPRINTER_INFO_2W>(info_buffer_.data());
    count_ = static_cast<int>(count);
}

//--------------------------------------------------------------------------------------------------
PrinterEnumerator::~PrinterEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool PrinterEnumerator::isAtEnd() const
{
    return current_ >= count_;
}

//--------------------------------------------------------------------------------------------------
void PrinterEnumerator::advance()
{
    ++current_;
}

//--------------------------------------------------------------------------------------------------
bool PrinterEnumerator::isDefault() const
{
    return info_[current_].pPrinterName == default_printer_;
}

//--------------------------------------------------------------------------------------------------
bool PrinterEnumerator::isShared() const
{
    return (info_[current_].Attributes & PRINTER_ATTRIBUTE_SHARED);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::name() const
{
    if (!info_[current_].pPrinterName)
        return QString();

    return QString::fromWCharArray(info_[current_].pPrinterName);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::shareName() const
{
    if (!info_[current_].pShareName)
        return QString();

    return QString::fromWCharArray(info_[current_].pShareName);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::portName() const
{
    if (!info_[current_].pPortName)
        return QString();

    return QString::fromWCharArray(info_[current_].pPortName);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::driverName() const
{
    if (!info_[current_].pDriverName)
        return QString();

    return QString::fromWCharArray(info_[current_].pDriverName);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::deviceName() const
{
    if (!info_[current_].pDevMode)
        return QString();

    return QString::fromWCharArray(info_[current_].pDevMode->dmDeviceName);
}

//--------------------------------------------------------------------------------------------------
int PrinterEnumerator::paperWidth() const
{
    if (!info_[current_].pDevMode)
        return 0;

    return info_[current_].pDevMode->dmPaperWidth / 10;
}

//--------------------------------------------------------------------------------------------------
int PrinterEnumerator::paperHeight() const
{
    if (!info_[current_].pDevMode)
        return 0;

    return info_[current_].pDevMode->dmPaperLength / 10;
}

//--------------------------------------------------------------------------------------------------
int PrinterEnumerator::printQuality() const
{
    if (!info_[current_].pDevMode)
        return 0;

    return info_[current_].pDevMode->dmPrintQuality;
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::printProcessor() const
{
    if (!info_[current_].pPrintProcessor)
        return QString();

    return QString::fromWCharArray(info_[current_].pPrintProcessor);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::dataType() const
{
    if (!info_[current_].pDatatype)
        return QString();

    return QString::fromWCharArray(info_[current_].pDatatype);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::serverName() const
{
    if (!info_[current_].pServerName)
        return QString();

    return QString::fromWCharArray(info_[current_].pServerName);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::location() const
{
    if (!info_[current_].pLocation)
        return QString();

    return QString::fromWCharArray(info_[current_].pLocation);
}

//--------------------------------------------------------------------------------------------------
QString PrinterEnumerator::comment() const
{
    if (!info_[current_].pComment)
        return QString();

    return QString::fromWCharArray(info_[current_].pComment);
}

//--------------------------------------------------------------------------------------------------
int PrinterEnumerator::jobsCount() const
{
    return static_cast<int>(info_[current_].cJobs);
}

} // namespace base
