//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_xml_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_XML_FILE_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/system_info/output.h"

#include <rapidxml.hpp>
#include <fstream>
#include <stack>

namespace aspia {

class OutputXmlFile : public Output
{
public:
    ~OutputXmlFile() = default;

    static std::unique_ptr<OutputXmlFile> Create(const FilePath& file_path);

protected:
    // Output implementation.
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(std::string_view name) final;
    void EndTableGroup() final;
    void StartTable(std::string_view name) final;
    void EndTable() final;
    void StartTableHeader() final;
    void EndTableHeader() final;
    void AddHeaderItem(std::string_view name, int width) final;
    void StartGroup(std::string_view name, Category::IconId icon_id) final;
    void EndGroup() final;
    void AddParam(Category::IconId icon_id,
                  std::string_view param,
                  std::string_view value,
                  std::string_view unit) final;
    void StartRow(Category::IconId icon_id) final;
    void EndRow() final;
    void AddValue(std::string_view value, std::string_view unit) final;

private:
    OutputXmlFile(std::ofstream file);

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
