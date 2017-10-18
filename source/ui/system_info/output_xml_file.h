//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_xml_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H

#include "base/macros.h"
#include "ui/system_info/output.h"

namespace aspia {

class OutputXmlFile : protected Output
{
public:
    OutputXmlFile(const std::wstring& file_path);
    ~OutputXmlFile() = default;

protected:
    // Output implementation.
    void BeginTable(const ColumnList& column_list) final;
    void EndTable() final;
    void BeginItemGroup(const std::string& name) final;
    void EndItemGroup() final;
    void AddItem(const std::string& parameter, const std::string& value) final;
    void AddItem(const std::string& value) final;

private:
    DISALLOW_COPY_AND_ASSIGN(OutputXmlFile);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H
