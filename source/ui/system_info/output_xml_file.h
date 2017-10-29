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

#include <rapidxml.hpp>
#include <fstream>
#include <stack>

namespace aspia {

class OutputXmlFile : protected Output
{
public:
    OutputXmlFile(const std::wstring& file_path);
    ~OutputXmlFile() = default;

protected:
    // Output implementation.
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(const std::string& name) final;
    void EndTableGroup() final;
    void StartTable(const std::string& name) final;
    void EndTable() final;
    void StartTableHeader() final;
    void EndTableHeader() final;
    void AddHeaderItem(const std::string& name, int width) final;
    void StartGroup(const std::string& name, Category::IconId icon_id) final;
    void EndGroup() final;
    void AddParam(Category::IconId icon_id,
                  const std::string& param,
                  const std::string& value,
                  const char* unit) final;
    void StartRow(Category::IconId icon_id) final;
    void EndRow() final;
    void AddValue(const std::string& value, const char* unit) final;

private:
    std::ofstream file_;
    rapidxml::xml_document<> doc_;
    rapidxml::xml_node<>* root_ = nullptr;
    rapidxml::xml_node<>* category_ = nullptr;
    std::stack<rapidxml::xml_node<>*> group_stack_;
    rapidxml::xml_node<>* row_ = nullptr;

    std::vector<std::string> column_list_;
    size_t current_column_ = 0;

    DISALLOW_COPY_AND_ASSIGN(OutputXmlFile);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H
