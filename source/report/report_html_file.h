//
// PROJECT:         Aspia
// FILE:            report/report_html_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__REPORT_HTML_FILE_H
#define _ASPIA_REPORT__REPORT_HTML_FILE_H

#include "base/macros.h"
#include "report/report.h"

#include <rapidxml.hpp>
#include <experimental/filesystem>
#include <fstream>

namespace aspia {

class ReportHtmlFile : public Report
{
public:
    ~ReportHtmlFile() = default;

    static std::unique_ptr<ReportHtmlFile> Create(
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
    ReportHtmlFile(std::ofstream file);

    std::ofstream file_;
    rapidxml::xml_document<> doc_;
    rapidxml::xml_node<>* html_ = nullptr;
    rapidxml::xml_node<>* body_ = nullptr;
    rapidxml::xml_node<>* table_ = nullptr;
    rapidxml::xml_node<>* tr_ = nullptr;
    int h_level_ = 1;
    int padding_ = 5;
    int column_count_ = 0;
    int current_column_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ReportHtmlFile);
};

} // namespace aspia

#endif // _ASPIA_REPORT__REPORT_HTML_FILE_H
