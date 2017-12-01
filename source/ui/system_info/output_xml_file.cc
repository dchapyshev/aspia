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

OutputXmlFile::OutputXmlFile(const std::wstring& file_path)
{
    file_.open(file_path);
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
    UNUSED_PARAMETER(name);
    // TODO
}

void OutputXmlFile::EndTableGroup()
{
    // TODO
}

void OutputXmlFile::StartTable(std::string_view name)
{
    category_ = doc_.allocate_node(rapidxml::node_element, "category");
    category_->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(std::data(name))));
}

void OutputXmlFile::EndTable()
{
    DCHECK(category_);
    root_->append_node(category_);
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

void OutputXmlFile::AddHeaderItem(std::string_view name, int width)
{
    UNUSED_PARAMETER(width);
    DCHECK(category_);

    column_list_.emplace_back(name);
}

void OutputXmlFile::StartGroup(std::string_view name, Category::IconId icon_id)
{
    UNUSED_PARAMETER(icon_id);
    DCHECK(category_);

    rapidxml::xml_node<>* group = doc_.allocate_node(rapidxml::node_element, "group");
    group->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(std::data(name))));

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

void OutputXmlFile::AddParam(Category::IconId icon_id,
                             std::string_view param,
                             std::string_view value,
                             std::string_view unit)
{
    UNUSED_PARAMETER(icon_id);
    DCHECK(category_);

    rapidxml::xml_node<>* param_node = doc_.allocate_node(rapidxml::node_element, "param");

    param_node->append_attribute(
        doc_.allocate_attribute("name", doc_.allocate_string(std::data(param))));

    if (!unit.empty())
    {
        // The unit of measure is an optional parameter.
        param_node->append_attribute(
            doc_.allocate_attribute("unit", doc_.allocate_string(std::data(unit))));
    }

    param_node->value(doc_.allocate_string(std::data(value)));

    if (group_stack_.empty())
    {
        category_->append_node(param_node);
    }
    else
    {
        group_stack_.top()->append_node(param_node);
    }
}

void OutputXmlFile::StartRow(Category::IconId icon_id)
{
    UNUSED_PARAMETER(icon_id);
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
            doc_.allocate_attribute("unit", doc_.allocate_string(std::data(unit))));
    }

    column->value(doc_.allocate_string(std::data(value)));

    row_->append_node(column);

    ++current_column_;
}

} // namespace aspia
