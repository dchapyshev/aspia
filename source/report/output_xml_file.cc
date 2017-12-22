//
// PROJECT:         Aspia
// FILE:            report/output_xml_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "report/output_xml_file.h"

#pragma warning(push, 3)
#include <rapidxml_print.hpp>
#pragma warning(pop)

namespace aspia {

OutputXmlFile::OutputXmlFile(std::ofstream file)
    : file_(std::move(file))
{
    // Nothing
}

// static
std::unique_ptr<OutputXmlFile> OutputXmlFile::Create(const FilePath& file_path)
{
    std::ofstream file;

    file.open(file_path, std::ofstream::out | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG(WARNING) << "Unable to create report file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<OutputXmlFile>(new OutputXmlFile(std::move(file)));
}

void OutputXmlFile::StartDocument()
{
    rapidxml::xml_node<>* decl = doc_.allocate_node(rapidxml::node_declaration);

    decl->append_attribute(doc_.allocate_attribute("version", "1.0"));
    decl->append_attribute(doc_.allocate_attribute("encoding", "utf-8"));

    doc_.append_node(decl);

    root_ = doc_.allocate_node(rapidxml::node_element, "document");

    root_->append_attribute(doc_.allocate_attribute("version", "1.0"));
    root_->append_attribute(doc_.allocate_attribute("generator", "Aspia"));
}

void OutputXmlFile::EndDocument()
{
    doc_.append_node(root_);
    file_ << doc_;
    file_.close();
    doc_.clear();
}

void OutputXmlFile::StartTableGroup(std::string_view name)
{
    rapidxml::xml_node<>* category_group =
        doc_.allocate_node(rapidxml::node_element, "category_group");
    category_group->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(name.data())));

    category_group_stack_.push(category_group);
}

void OutputXmlFile::EndTableGroup()
{
    rapidxml::xml_node<>* category_group = category_group_stack_.top();
    category_group_stack_.pop();

    if (category_group_stack_.empty())
    {
        root_->append_node(category_group);
    }
    else
    {
        category_group_stack_.top()->append_node(category_group);
    }
}

void OutputXmlFile::StartTable(Category* category)
{
    category_ = doc_.allocate_node(rapidxml::node_element, "category");
    category_->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(category->Name())));
}

void OutputXmlFile::EndTable()
{
    DCHECK(category_);

    if (category_group_stack_.empty())
    {
        root_->append_node(category_);
    }
    else
    {
        category_group_stack_.top()->append_node(category_);
    }

    category_ = nullptr;
    column_list_.clear();
}

void OutputXmlFile::AddColumns(const ColumnList& column_list)
{
    DCHECK(category_);

    for (const auto& column : column_list)
    {
        column_list_.emplace_back(column.first);
    }
}

void OutputXmlFile::StartGroup(std::string_view name)
{
    DCHECK(category_);

    rapidxml::xml_node<>* group = doc_.allocate_node(rapidxml::node_element, "group");
    group->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(name.data())));

    group_stack_.push(group);
}

void OutputXmlFile::EndGroup()
{
    DCHECK(category_);

    rapidxml::xml_node<>* group = group_stack_.top();
    group_stack_.pop();

    if (group_stack_.empty())
    {
        category_->append_node(group);
    }
    else
    {
        group_stack_.top()->append_node(group);
    }
}

void OutputXmlFile::AddParam(std::string_view param, const Value& value)
{
    DCHECK(category_);

    rapidxml::xml_node<>* param_node = doc_.allocate_node(rapidxml::node_element, "param");

    param_node->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(param.data())));

    if (value.HasUnit())
    {
        // The unit of measure is an optional parameter.
        param_node->append_attribute(
            doc_.allocate_attribute("unit", doc_.allocate_string(value.Unit().data())));
    }

    param_node->value(doc_.allocate_string(ValueToString(value).data()));

    if (group_stack_.empty())
    {
        category_->append_node(param_node);
    }
    else
    {
        group_stack_.top()->append_node(param_node);
    }
}

void OutputXmlFile::StartRow()
{
    DCHECK(category_);
    DCHECK(!row_);

    row_ = doc_.allocate_node(rapidxml::node_element, "row");
    current_column_ = 0;
}

void OutputXmlFile::EndRow()
{
    DCHECK(category_);
    DCHECK(row_);

    if (group_stack_.empty())
    {
        category_->append_node(row_);
    }
    else
    {
        group_stack_.top()->append_node(row_);
    }

    row_ = nullptr;
}

void OutputXmlFile::AddValue(const Value& value)
{
    DCHECK(category_);
    DCHECK(row_);
    DCHECK(!column_list_.empty());
    DCHECK(current_column_ < column_list_.size());

    rapidxml::xml_node<>* column = doc_.allocate_node(rapidxml::node_element, "column");

    const std::string& column_name = column_list_[current_column_];

    column->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(column_name.c_str())));

    if (value.HasUnit())
    {
        // The unit of measure is an optional parameter.
        column->append_attribute(
            doc_.allocate_attribute("unit", doc_.allocate_string(value.Unit().data())));
    }

    column->value(doc_.allocate_string(ValueToString(value).data()));

    row_->append_node(column);

    ++current_column_;
}

// static
std::string OutputXmlFile::ValueToString(const Value& value)
{
    switch (value.type())
    {
        case Value::Type::BOOL:
            return value.ToBool() ? "true" : "false";

        case Value::Type::STRING:
            return std::string(value.ToString());

        case Value::Type::INT32:
            return std::to_string(value.ToInt32());

        case Value::Type::UINT32:
            return std::to_string(value.ToUint32());

        case Value::Type::INT64:
            return std::to_string(value.ToInt64());

        case Value::Type::UINT64:
            return std::to_string(value.ToUint64());

        case Value::Type::DOUBLE:
            return std::to_string(value.ToDouble());

        case Value::Type::MEMORY_SIZE:
            return std::to_string(value.ToMemorySize());

        default:
        {
            DLOG(FATAL) << "Unhandled value type: " << static_cast<int>(value.type());
            return std::string();
        }
    }
}

} // namespace aspia
