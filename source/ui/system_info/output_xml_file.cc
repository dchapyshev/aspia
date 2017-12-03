//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_xml_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/output_xml_file.h"

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
    root_->append_attribute(doc_.allocate_attribute("generator", "Aspia Remote Desktop"));
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

void OutputXmlFile::StartTable(std::string_view name, TableType /* table_type */)
{
    category_ = doc_.allocate_node(rapidxml::node_element, "category");
    category_->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(name.data())));
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

void OutputXmlFile::StartTableHeader()
{
    // Nothing
}

void OutputXmlFile::EndTableHeader()
{
    // Nothing
}

void OutputXmlFile::AddHeaderItem(std::string_view name, int /* width */)
{
    DCHECK(category_);

    column_list_.emplace_back(name);
}

void OutputXmlFile::StartGroup(std::string_view name, Category::IconId /* icon_id */)
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

void OutputXmlFile::AddParam(Category::IconId /* icon_id */,
                             std::string_view param,
                             std::string_view value,
                             std::string_view unit)
{
    DCHECK(category_);

    rapidxml::xml_node<>* param_node = doc_.allocate_node(rapidxml::node_element, "param");

    param_node->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(param.data())));

    if (!unit.empty())
    {
        // The unit of measure is an optional parameter.
        param_node->append_attribute(
            doc_.allocate_attribute("unit", doc_.allocate_string(unit.data())));
    }

    param_node->value(doc_.allocate_string(value.data()));

    if (group_stack_.empty())
    {
        category_->append_node(param_node);
    }
    else
    {
        group_stack_.top()->append_node(param_node);
    }
}

void OutputXmlFile::StartRow(Category::IconId /* icon_id */)
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

    category_->append_node(row_);
    row_ = nullptr;
}

void OutputXmlFile::AddValue(std::string_view value, std::string_view unit)
{
    DCHECK(category_);
    DCHECK(row_);
    DCHECK(!column_list_.empty());
    DCHECK(current_column_ < column_list_.size());

    rapidxml::xml_node<>* column = doc_.allocate_node(rapidxml::node_element, "column");

    const std::string& column_name = column_list_[current_column_];

    column->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(column_name.c_str())));

    if (!unit.empty())
    {
        // The unit of measure is an optional parameter.
        column->append_attribute(
            doc_.allocate_attribute("unit", doc_.allocate_string(unit.data())));
    }

    column->value(doc_.allocate_string(value.data()));

    row_->append_node(column);

    ++current_column_;
}

} // namespace aspia
