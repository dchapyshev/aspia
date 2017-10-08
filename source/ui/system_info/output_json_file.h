//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_json_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H

#include "ui/system_info/output.h"

namespace aspia {

class OutputJsonFile : protected Output
{
public:
    OutputJsonFile(const std::wstring& file_path);
    ~OutputJsonFile() = default;

protected:
    // Output implementation.
    void BeginTable(const ColumnList& column_list) final;
    void EndTable() final;
    void BeginItemGroup(const std::string& name) final;
    void EndItemGroup() final;
    void AddItem(const std::string& parameter, const std::string& value) final;
    void AddItem(const std::string& value) final;

private:
    DISALLOW_COPY_AND_ASSIGN(OutputJsonFile);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H
