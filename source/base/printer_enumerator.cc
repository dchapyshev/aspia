//
// PROJECT:         Aspia
// FILE:            base/printer_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/printer_enumerator.h"
#include "base/strings/unicode.h"
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

std::string PrinterEnumerator::GetName() const
{
    return UTF8fromUNICODE(printers_info_[current_].pPrinterName);
}

bool PrinterEnumerator::IsDefault() const
{
    if (!printers_info_[current_].pPrinterName || default_printer_name_.empty())
        return false;

    if (default_printer_name_ == printers_info_[current_].pPrinterName)
        return true;

    return false;
}

bool PrinterEnumerator::IsShared() const
{
    return printers_info_[current_].Attributes & PRINTER_ATTRIBUTE_SHARED;
}

std::string PrinterEnumerator::GetShareName() const
{
    if (!IsShared())
        return std::string();

    return UTF8fromUNICODE(printers_info_[current_].pShareName);
}

std::string PrinterEnumerator::GetPortName() const
{
    return UTF8fromUNICODE(printers_info_[current_].pPortName);
}

std::string PrinterEnumerator::GetDriverName() const
{
    return UTF8fromUNICODE(printers_info_[current_].pDriverName);
}

std::string PrinterEnumerator::GetDeviceName() const
{
    if (!printers_info_[current_].pDevMode)
        return std::string();

    return UTF8fromUNICODE(printers_info_[current_].pDevMode->dmDeviceName);
}

std::string PrinterEnumerator::GetPrintProcessor() const
{
    return UTF8fromUNICODE(printers_info_[current_].pPrintProcessor);
}

std::string PrinterEnumerator::GetDataType() const
{
    return UTF8fromUNICODE(printers_info_[current_].pDatatype);
}

std::string PrinterEnumerator::GetServerName() const
{
    return UTF8fromUNICODE(printers_info_[current_].pServerName);
}

std::string PrinterEnumerator::GetLocation() const
{
    return UTF8fromUNICODE(printers_info_[current_].pLocation);
}

std::string PrinterEnumerator::GetComment() const
{
    return UTF8fromUNICODE(printers_info_[current_].pComment);
}

int PrinterEnumerator::GetJobsCount() const
{
    return printers_info_[current_].cJobs;
}

int PrinterEnumerator::GetPaperWidth() const
{
    if (!printers_info_[current_].pDevMode)
        return 0;

    return printers_info_[current_].pDevMode->dmPaperWidth / 10;
}

int PrinterEnumerator::GetPaperLength() const
{
    if (!printers_info_[current_].pDevMode)
        return 0;

    return printers_info_[current_].pDevMode->dmPaperLength / 10;
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
