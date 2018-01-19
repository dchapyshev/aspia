//
// PROJECT:         Aspia
// FILE:            report/report_json_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__REPORT_JSON_FILE_H
#define _ASPIA_REPORT__REPORT_JSON_FILE_H

#include "base/macros.h"
#include "report/report.h"

#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_SSE2
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <experimental/filesystem>
#include <fstream>

namespace aspia {

class ReportJsonFile : public Report
{
public:
    ~ReportJsonFile() = default;

    static std::unique_ptr<ReportJsonFile> Create(
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
    ReportJsonFile(std::ofstream file);

    void WriteValue(const Value& value);

    std::ofstream file_;
    rapidjson::StringBuffer buffer_;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer_;

    std::vector<std::string> column_list_;
    int column_index_ = 0;
    Category* category_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ReportJsonFile);
};

} // namespace aspia

#endif // _ASPIA_REPORT__REPORT_JSON_FILE_H
