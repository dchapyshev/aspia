//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/printer_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/logging.h"

namespace aspia {

PrinterEnumerator::PrinterEnumerator()
{
    const DWORD flags = PRINTER_ENUM_FAVORITE | PRINTER_ENUM_LOCAL | PRINTER_ENUM_NETWORK;
    DWORD bytes_needed = 0;

    if (EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes_needed, &count_) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(ERROR) << "Unexpected return value: " << GetLastSystemErrorString();
        return;
    }

    printers_info_buffer_ = std::make_unique<uint8_t[]>(bytes_needed);
    printers_info_ = reinterpret_cast<PPRINTER_INFO_2>(printers_info_buffer_.get());

    if (!EnumPrintersW(flags, nullptr, 2, printers_info_buffer_.get(), bytes_needed,
                       &bytes_needed, &count_))
    {
        printers_info_buffer_.reset();
        printers_info_ = nullptr;
        count_ = 0;
        return;
    }

    DWORD characters_count = 0;

    if (!GetDefaultPrinterW(nullptr, &characters_count) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        default_printer_name_.resize(characters_count);

        if (!GetDefaultPrinterW(&default_printer_name_[0], &characters_count))
        {
            LOG(ERROR) << "GetDefaultPrinterW() failed: " << GetLastSystemErrorString();
        }
    }
}

bool PrinterEnumerator::IsAtEnd() const
{
    return current_ >= count_;
}

void PrinterEnumerator::Advance()
{
    ++current_;
}

std::wstring PrinterEnumerator::GetName() const
{
    if (!printers_info_[current_].pPrinterName || !printers_info_[current_].pPrinterName[0])
        return std::wstring();

    return printers_info_[current_].pPrinterName;
}

bool PrinterEnumerator::IsDefault() const
{
    std::wstring name = GetName();

    if (name.empty() || default_printer_name_.empty())
        return false;

    if (name == default_printer_name_)
        return true;

    return false;
}

bool PrinterEnumerator::IsShared() const
{
    return printers_info_[current_].Attributes & PRINTER_ATTRIBUTE_SHARED;
}

std::wstring PrinterEnumerator::GetShareName() const
{
    if (!IsShared())
        return std::wstring();

    if (!printers_info_[current_].pShareName || !printers_info_[current_].pShareName[0])
        return std::wstring();

    return printers_info_[current_].pShareName;
}

std::wstring PrinterEnumerator::GetPortName() const
{
    if (!printers_info_[current_].pPortName || !printers_info_[current_].pPortName[0])
        return std::wstring();

    return printers_info_[current_].pPortName;
}

std::wstring PrinterEnumerator::GetDriverName() const
{
    if (!printers_info_[current_].pDriverName || !printers_info_[current_].pDriverName[0])
        return std::wstring();

    return printers_info_[current_].pDriverName;
}

std::wstring PrinterEnumerator::GetDeviceName() const
{
    if (!printers_info_[current_].pDevMode || !printers_info_[current_].pDevMode->dmDeviceName ||
        !printers_info_[current_].pDevMode->dmDeviceName[0])
        return std::wstring();

    return printers_info_[current_].pDevMode->dmDeviceName;
}

std::wstring PrinterEnumerator::GetPrintProcessor() const
{
    if (!printers_info_[current_].pPrintProcessor || !printers_info_[current_].pPrintProcessor[0])
        return std::wstring();

    return printers_info_[current_].pPrintProcessor;
}

std::wstring PrinterEnumerator::GetDataType() const
{
    if (!printers_info_[current_].pDatatype || !printers_info_[current_].pDatatype[0])
        return std::wstring();

    return printers_info_[current_].pDatatype;
}

std::wstring PrinterEnumerator::GetServerName() const
{
    if (!printers_info_[current_].pServerName || !printers_info_[current_].pServerName[0])
        return std::wstring();

    return printers_info_[current_].pServerName;
}

std::wstring PrinterEnumerator::GetLocation() const
{
    if (!printers_info_[current_].pLocation || !printers_info_[current_].pLocation[0])
        return std::wstring();

    return printers_info_[current_].pLocation;
}

std::wstring PrinterEnumerator::GetComment() const
{
    if (!printers_info_[current_].pComment || !printers_info_[current_].pComment[0])
        return std::wstring();

    return printers_info_[current_].pComment;
}

int PrinterEnumerator::GetJobsCount() const
{
    return printers_info_[current_].cJobs;
}

int PrinterEnumerator::GetPaperWidth() const
{
    if (!printers_info_[current_].pDevMode)
        return 0;

    return printers_info_[current_].pDevMode->dmPaperWidth;
}

int PrinterEnumerator::GetPaperLength() const
{
    if (!printers_info_[current_].pDevMode)
        return 0;

    return printers_info_[current_].pDevMode->dmPaperLength;
}

int PrinterEnumerator::GetPrintQuality() const
{
    if (!printers_info_[current_].pDevMode)
        return 0;

    return printers_info_[current_].pDevMode->dmPrintQuality;
}

PrinterEnumerator::Orientation PrinterEnumerator::GetOrientation() const
{
    if (!printers_info_[current_].pDevMode)
        return Orientation::UNKNOWN;

    switch (printers_info_[current_].pDevMode->dmOrientation)
    {
        case DMORIENT_PORTRAIT:
            return Orientation::PORTRAIT;

        case DMORIENT_LANDSCAPE:
            return Orientation::LANDSCAPE;

        default:
            return Orientation::UNKNOWN;
    }
}

} // namespace aspia
