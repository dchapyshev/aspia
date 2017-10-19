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
    bool StartDocument(const std::string& name);
    bool EndDocument();

    bool StartTable(const std::string& name);
    bool EndTable();

    bool StartTableHeader();
    bool EndTableHeader();
    bool AddHeaderItem(const std::string& name, int width);

    bool StartGroup(const std::string& name, Category::IconId icon_id);
    bool EndGroup();
    bool AddParam(Category::IconId icon_id,
                  const std::string& param,
                  const std::string& value,
                  const char* unit = nullptr);

    bool StartRow(Category::IconId icon_id);
    bool EndRow();
    bool AddValue(const std::string& value, const char* unit = nullptr);

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
