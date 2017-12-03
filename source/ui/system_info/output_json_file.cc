//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_json_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/output_json_file.h"

namespace aspia {

OutputJsonFile::OutputJsonFile(std::ofstream file)
    : writer_(buffer_),
      file_(std::move(file))
{
    writer_.SetIndent('\t', 1);
}

// static
std::unique_ptr<OutputJsonFile> OutputJsonFile::Create(const FilePath& file_path)
{
    std::ofstream file;

    file.open(file_path, std::ofstream::out | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG(WARNING) << "Unable to create report file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<OutputJsonFile>(new OutputJsonFile(std::move(file)));
}

void OutputJsonFile::StartDocument()
{
    writer_.StartObject();
}

void OutputJsonFile::EndDocument()
{
    writer_.EndObject();
    file_.write(buffer_.GetString(), buffer_.GetSize());
    file_.close();
}

void OutputJsonFile::StartTableGroup(std::string_view name)
{
    writer_.String(name.data());
    writer_.StartObject();
}

void OutputJsonFile::EndTableGroup()
{
    writer_.EndObject();
}

void OutputJsonFile::StartTable(std::string_view name, TableType table_type)
{
    table_type_ = table_type;

    writer_.String(name.data());
    writer_.StartObject();

    if (table_type_ == TableType::LIST)
        writer_.StartArray();
}

void OutputJsonFile::EndTable()
{
    if (table_type_ == TableType::LIST)
        writer_.EndArray();

    writer_.EndObject();
    column_list_.clear();
}

void OutputJsonFile::StartTableHeader()
{
    // Nothing
}

void OutputJsonFile::EndTableHeader()
{
    // Nothing
}

void OutputJsonFile::AddHeaderItem(std::string_view name, int /* width */)
{
    column_list_.emplace_back(name);
}

void OutputJsonFile::StartGroup(std::string_view name, Category::IconId /* icon_id */)
{
    writer_.String(name.data());
    writer_.StartObject();
}

void OutputJsonFile::EndGroup()
{
    writer_.EndObject();
}

void OutputJsonFile::AddParam(Category::IconId /* icon_id */,
                              std::string_view param,
                              std::string_view value,
                              std::string_view unit)
{
    if (unit.empty())
    {
        writer_.Key(param.data());
        writer_.String(value.data());
    }
    else
    {
        writer_.String(param.data());
        writer_.StartObject();

        writer_.Key("value");
        writer_.String(value.data());

        writer_.Key("unit");
        writer_.String(unit.data());

        writer_.EndObject();
    }
}

void OutputJsonFile::StartRow(Category::IconId /* icon_id */)
{
    writer_.StartObject();
    column_index_ = 0;
}

void OutputJsonFile::EndRow()
{
    writer_.EndObject();
}

void OutputJsonFile::AddValue(std::string_view value, std::string_view unit)
{
    if (unit.empty())
    {
        writer_.Key(column_list_[column_index_].data());
        writer_.String(value.data());
    }
    else
    {
        writer_.String(column_list_[column_index_].data());
        writer_.StartObject();

        writer_.Key("value");
        writer_.String(value.data());

        writer_.Key("unit");
        writer_.String(unit.data());

        writer_.EndObject();
    }

    ++column_index_;
}

} // namespace aspia
