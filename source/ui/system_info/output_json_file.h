//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_json_file.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H
#define _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/system_info/output.h"

#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_SSE2
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>

namespace aspia {

class OutputJsonFile : public Output
{
public:
    ~OutputJsonFile() = default;

    static std::unique_ptr<OutputJsonFile> Create(const FilePath& file_path);

protected:
    // Output implementation.
    void StartDocument() final;
    void EndDocument() final;
    void StartTableGroup(std::string_view name) final;
    void EndTableGroup() final;
    void StartTable(Category* category, TableType table_type) final;
    void EndTable() final;
    void Add(const ColumnList& column_list) final;
    void StartGroup(std::string_view name) final;
    void EndGroup() final;
    void AddParam(std::string_view param, const Value& value) final;
    void StartRow() final;
    void EndRow() final;
    void AddValue(const Value& value) final;

private:
    OutputJsonFile(std::ofstream file);

    std::ofstream file_;
    rapidjson::StringBuffer buffer_;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer_;

    std::vector<std::string> column_list_;
    int column_index_ = 0;
    TableType table_type_;

    DISALLOW_COPY_AND_ASSIGN(OutputJsonFile);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__OUTPUT_JSON_FILE_H
