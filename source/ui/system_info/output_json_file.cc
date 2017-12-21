//
// PROJECT:         Aspia
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

void OutputJsonFile::StartTable(Category* category)
{
    DCHECK(!category_);
    category_ = category;
    DCHECK(category_);

    writer_.String(category->Name());
    writer_.StartObject();

    if (category_->type() == Category::Type::INFO_LIST)
        writer_.StartArray();
}

void OutputJsonFile::EndTable()
{
    DCHECK(category_);

    if (category_->type() == Category::Type::INFO_LIST)
        writer_.EndArray();

    writer_.EndObject();
    column_list_.clear();

    category_ = nullptr;
}

void OutputJsonFile::AddColumns(const ColumnList& column_list)
{
    for (const auto& column : column_list)
    {
        column_list_.emplace_back(column.first);
    }
}

void OutputJsonFile::StartGroup(std::string_view name)
{
    writer_.String(name.data());
    writer_.StartObject();
}

void OutputJsonFile::EndGroup()
{
    writer_.EndObject();
}

void OutputJsonFile::AddParam(std::string_view param, const Value& value)
{
    if (!value.HasUnit())
    {
        writer_.Key(param.data());
        WriteValue(value);
    }
    else
    {
        writer_.String(param.data());
        writer_.StartObject();

        writer_.Key("value");
        WriteValue(value);

        writer_.Key("unit");
        writer_.String(value.Unit().data());

        writer_.EndObject();
    }
}

void OutputJsonFile::StartRow()
{
    writer_.StartObject();
    column_index_ = 0;
}

void OutputJsonFile::EndRow()
{
    writer_.EndObject();
}

void OutputJsonFile::AddValue(const Value& value)
{
    if (!value.HasUnit())
    {
        writer_.Key(column_list_[column_index_].data());
        WriteValue(value);
    }
    else
    {
        writer_.String(column_list_[column_index_].data());
        writer_.StartObject();

        writer_.Key("value");
        WriteValue(value);

        writer_.Key("unit");
        writer_.String(value.Unit().data());

        writer_.EndObject();
    }

    ++column_index_;
}

void OutputJsonFile::WriteValue(const Value& value)
{
    switch (value.type())
    {
        case Value::Type::STRING:
            writer_.String(value.ToString().data());
            break;

        case Value::Type::BOOL:
            writer_.Bool(value.ToBool());
            break;

        case Value::Type::UINT32:
            writer_.Uint(value.ToUint32());
            break;

        case Value::Type::INT32:
            writer_.Int(value.ToInt32());
            break;

        case Value::Type::UINT64:
            writer_.Uint64(value.ToUint64());
            break;

        case Value::Type::INT64:
            writer_.Int64(value.ToInt64());
            break;

        case Value::Type::DOUBLE:
            writer_.Double(value.ToDouble());
            break;

        case Value::Type::MEMORY_SIZE:
            writer_.Double(value.ToMemorySize());
            break;

        default:
            DLOG(FATAL) << "Unhandled value type: " << static_cast<int>(value.type());
            break;
    }
}

} // namespace aspia
