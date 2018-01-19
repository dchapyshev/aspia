//
// PROJECT:         Aspia
// FILE:            report/report_xml_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__REPORT_XML_FILE_H
#define _ASPIA_REPORT__REPORT_XML_FILE_H

#include "base/macros.h"
#include "report/report.h"

#include <rapidxml.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <stack>

namespace aspia {

class ReportXmlFile : public Report
{
public:
    ~ReportXmlFile() = default;

    static std::unique_ptr<ReportXmlFile> Create(
        const std::experimental::filesystem::path& file_path);

protected:
    // Report implementation.
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(std::string_view name) final;
    void EndTableGroup() final;
    void StartTable(Category* category) final;
    void EndTable() final;
    void AddColumns(const ColumnList& column_list) final;
    void StartGroup(std::string_view name) final;
    void EndGroup() final;
    void AddParam(std::string_view param, const Value& value) final;
    void StartRow() final;
    void EndRow() final;
    void AddValue(const Value& value) final;

private:
    ReportXmlFile(std::ofstream file);

    static std::string ValueToString(const Value& value);

    std::ofstream file_;
    rapidxml::xml_document<> doc_;
    rapidxml::xml_node<>* root_ = nullptr;
    rapidxml::xml_node<>* category_ = nullptr;
    std::stack<rapidxml::xml_node<>*> group_stack_;
    rapidxml::xml_node<>* row_ = nullptr;
    std::stack<rapidxml::xml_node<>*> category_group_stack_;

    std::vector<std::string> column_list_;
    size_t current_column_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ReportXmlFile);
};

} // namespace aspia

#endif // _ASPIA_REPORT__REPORT_XML_FILE_H
