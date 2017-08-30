//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/printer_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PRINTER_ENUMERATOR_H
#define _ASPIA_BASE__PRINTER_ENUMERATOR_H

#include "base/macros.h"

#include <winspool.h>
#include <memory>
#include <string>

namespace aspia {

class PrinterEnumerator
{
public:
    PrinterEnumerator();
    ~PrinterEnumerator() = default;

    bool IsAtEnd() const;
    void Advance();

    std::wstring GetName() const;
    bool IsDefault() const;
    bool IsShared() const;
    std::wstring GetShareName() const;
    std::wstring GetPortName() const;
    std::wstring GetDriverName() const;
    std::wstring GetDeviceName() const;
    std::wstring GetPrintProcessor() const;
    std::wstring GetDataType() const;
    std::wstring GetServerName() const;
    std::wstring GetLocation() const;
    std::wstring GetComment() const;
    int GetJobsCount() const;
    int GetPaperWidth() const;
    int GetPaperLength() const;
    int GetPrintQuality() const;

    enum class Orientation
    {
        UNKNOWN   = 0,
        PORTRAIT  = 1,
        LANDSCAPE = 2
    };

    Orientation GetOrientation() const;

private:
    std::wstring default_printer_name_;
    std::unique_ptr<uint8_t[]> printers_info_buffer_;
    PPRINTER_INFO_2 printers_info_ = nullptr;
    DWORD count_ = 0;
    DWORD current_ = 0;

    DISALLOW_COPY_AND_ASSIGN(PrinterEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__PRINTER_ENUMERATOR_H
