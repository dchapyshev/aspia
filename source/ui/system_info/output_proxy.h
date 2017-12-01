//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_PROXY_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_PROXY_H

#include "base/macros.h"
#include "ui/system_info/output.h"

#include <mutex>

namespace aspia {

class OutputProxy
{
public:
    bool StartDocument();
    bool EndDocument();

    bool StartTableGroup(std::string_view name);
    bool EndTableGroup();

    bool StartTable(std::string_view name);
    bool EndTable();

    bool StartTableHeader();
    bool EndTableHeader();
    bool AddHeaderItem(std::string_view name, int width);

    bool StartGroup(std::string_view name, Category::IconId icon_id);
    bool EndGroup();
    bool AddParam(Category::IconId icon_id,
                  std::string_view param,
                  std::string_view value,
                  std::string_view unit);
    bool AddParam(Category::IconId icon_id,
                  std::string_view param,
                  std::string_view value);

    bool StartRow(Category::IconId icon_id);
    bool EndRow();
    bool AddValue(std::string_view value, std::string_view unit);
    bool AddValue(std::string_view value);

private:
    friend class Output;

    explicit OutputProxy(Output* output);

    // Called directly by Output::~Output.
    void WillDestroyCurrentOutput();

    Output* output_;
    mutable std::mutex output_lock_;

    DISALLOW_COPY_AND_ASSIGN(OutputProxy);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_PROXY_H
