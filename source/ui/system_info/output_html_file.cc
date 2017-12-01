//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_html_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/output_html_file.h"

#pragma warning(push, 3)
#include <rapidxml_print.hpp>
#pragma warning(pop)

namespace aspia {

static const char kCssStyle[] =
    "body {"
    "color: #353535;"
    "font-family: Tahoma,Arial,Verdana;"
    "}"
    "h1 {"
    "font-size: 14px;"
    "}"
    "table {"
    "background-color: #F9F9FC;"
    "border: 1px solid #E2E2E0;"
    "font-size: 12px;"
    "}"
    "tr {"
    "border-bottom: 1px solid #E2E2E0;"
    "}"
    "tbody tr: hover {"
    "background-color:#F2F2F8;"
    "}"
    "th,td {"
    "padding: 5px;"
    "}"
    "th { "
    "background-color: #DCDCED;"
    "font-weight:bold;"
    "}";

OutputHtmlFile::OutputHtmlFile(const std::wstring& file_path)
{
    file_.open(file_path);
}

void OutputHtmlFile::StartDocument()
{
    html_ = doc_.allocate_node(rapidxml::node_element, "html");

    rapidxml::xml_node<>* head = doc_.allocate_node(rapidxml::node_element, "head");

    rapidxml::xml_node<>* meta = doc_.allocate_node(rapidxml::node_element, "meta");

    meta->append_attribute(doc_.allocate_attribute("http-equiv", "content-type"));
    meta->append_attribute(doc_.allocate_attribute("content", "text/html"));
    meta->append_attribute(doc_.allocate_attribute("charset", "UTF-8"));

    head->append_node(meta);

    meta = doc_.allocate_node(rapidxml::node_element, "meta");
    meta->append_attribute(doc_.allocate_attribute("generator", "Aspia Remote Desktop"));

    head->append_node(meta);

    rapidxml::xml_node<>* title = doc_.allocate_node(rapidxml::node_element, "title");
    title->value("Report");

    head->append_node(title);

    rapidxml::xml_node<>* style = doc_.allocate_node(rapidxml::node_element, "style");
    style->append_attribute(doc_.allocate_attribute("type", "text/css"));
    style->value(kCssStyle);

    head->append_node(style);
    html_->append_node(head);

    body_ = doc_.allocate_node(rapidxml::node_element, "body");
}

void OutputHtmlFile::EndDocument()
{
    DCHECK(html_);
    DCHECK(body_);

    html_->append_node(body_);
    doc_.append_node(html_);

    file_ << doc_;
    file_.close();
    doc_.clear();
}

void OutputHtmlFile::StartTableGroup(std::string_view name)
{
    UNUSED_PARAMETER(name);
    // TODO
}

void OutputHtmlFile::EndTableGroup()
{
    // TODO
}

void OutputHtmlFile::StartTable(std::string_view name)
{
    DCHECK(body_);
    DCHECK(!table_);

    rapidxml::xml_node<>* h1 = doc_.allocate_node(rapidxml::node_element, "h1");
    h1->value(doc_.allocate_string(std::data(name)));
    body_->append_node(h1);

    table_ = doc_.allocate_node(rapidxml::node_element, "table");
}

void OutputHtmlFile::EndTable()
{
    DCHECK(body_);
    DCHECK(table_);

    body_->append_node(table_);
    table_ = nullptr;
}

void OutputHtmlFile::StartTableHeader()
{
    DCHECK(table_);
    tr_ = doc_.allocate_node(rapidxml::node_element, "tr");
}

void OutputHtmlFile::EndTableHeader()
{
    DCHECK(table_);
    DCHECK(tr_);

    table_->append_node(tr_);
    tr_ = nullptr;
}

void OutputHtmlFile::AddHeaderItem(std::string_view name, int width)
{
    UNUSED_PARAMETER(width);
    DCHECK(table_);
    DCHECK(tr_);

    rapidxml::xml_node<>* th = doc_.allocate_node(rapidxml::node_element, "th");
    th->value(doc_.allocate_string(std::data(name)));
    tr_->append_node(th);
}

void OutputHtmlFile::StartGroup(std::string_view name, Category::IconId icon_id)
{
    UNUSED_PARAMETER(icon_id);
    DCHECK(table_);

    rapidxml::xml_node<>* td1 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->append_attribute(doc_.allocate_attribute("style", "font-weight: bold;"));
    td1->value(doc_.allocate_string(std::data(name), name.length()));

    rapidxml::xml_node<>* td2 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->value(doc_.allocate_string("&nbsp;"));

    rapidxml::xml_node<>* tr = doc_.allocate_node(rapidxml::node_element, "tr");
    tr->append_node(td1);
    tr->append_node(td2);

    table_->append_node(tr);
}

void OutputHtmlFile::EndGroup()
{
    // Nothing
}

void OutputHtmlFile::AddParam(Category::IconId icon_id,
                              std::string_view param,
                              std::string_view value,
                              std::string_view unit)
{
    UNUSED_PARAMETER(icon_id);
    DCHECK(table_);

    rapidxml::xml_node<>* td1 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->append_attribute(doc_.allocate_attribute("style", "font-weight: bold;"));
    td1->value(doc_.allocate_string(std::data(param), param.length()));

    std::string value_with_unit(value);

    if (!unit.empty())
    {
        value_with_unit.append(" ");
        value_with_unit.append(value);
    }

    rapidxml::xml_node<>* td2 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->value(doc_.allocate_string(value_with_unit.c_str(), value_with_unit.length()));

    rapidxml::xml_node<>* tr = doc_.allocate_node(rapidxml::node_element, "tr");
    tr->append_node(td1);
    tr->append_node(td2);

    table_->append_node(tr);
}

void OutputHtmlFile::StartRow(Category::IconId icon_id)
{
    UNUSED_PARAMETER(icon_id);
    DCHECK(table_);
    DCHECK(!tr_);

    tr_ = doc_.allocate_node(rapidxml::node_element, "tr");
}

void OutputHtmlFile::EndRow()
{
    DCHECK(table_);
    DCHECK(tr_);

    table_->append_node(tr_);
    tr_ = nullptr;
}

void OutputHtmlFile::AddValue(std::string_view value, std::string_view unit)
{
    DCHECK(table_);
    DCHECK(tr_);

    std::string value_with_unit(value);

    if (!unit.empty())
    {
        value_with_unit.append(" ");
        value_with_unit.append(value);
    }

    rapidxml::xml_node<>* td = doc_.allocate_node(rapidxml::node_element, "td");
    td->value(doc_.allocate_string(value_with_unit.c_str(), value_with_unit.length()));

    tr_->append_node(td);
}

} // namespace aspia
