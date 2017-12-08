//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_html_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_HTML_FILE_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_HTML_FILE_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/system_info/output.h"

#include <rapidxml.hpp>
#include <fstream>

namespace aspia {

class OutputHtmlFile : public Output
{
public:
    ~OutputHtmlFile() = default;

    static std::unique_ptr<OutputHtmlFile> Create(const FilePath& file_path);

protected:
    // Output implementation.
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(std::string_view name) final;
    void EndTableGroup() final;
    void StartTable(std::string_view name, TableType table_type) final;
    void EndTable() final;
    void Add(const ColumnList& column_list) final;
    void StartGroup(std::string_view name, Category::IconId icon_id) final;
    void EndGroup() final;
    void AddParam(Category::IconId icon_id,
                  std::string_view param,
                  const Value& value) final;
    void StartRow(Category::IconId icon_id) final;
    void EndRow() final;
    void AddValue(const Value& value) final;

private:
    OutputHtmlFile(std::ofstream file);

    std::ofstream file_;
    rapidxml::xml_document<> doc_;
    rapidxml::xml_node<>* html_ = nullptr;
    rapidxml::xml_node<>* body_ = nullptr;
    rapidxml::xml_node<>* table_ = nullptr;
    rapidxml::xml_node<>* tr_ = nullptr;
    int h_level_ = 1;
    int padding_ = 5;

    DISALLOW_COPY_AND_ASSIGN(OutputHtmlFile);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_HTML_FILE_H
