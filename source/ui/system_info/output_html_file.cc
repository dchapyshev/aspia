//
// PROJECT:         Aspia
// FILE:            ui/system_info/output_html_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
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
    "h1 { font-size: 18px; }"
    "h2 { font-size: 16px; }"
    "h3 { font-size: 14px; }"
    "h4 { font-size: 12px; }"
    "h5 { font-size: 10px; }"
    "h6 { font-size: 8px; }"
    "table {"
        "background-color: #F9F9FC;"
        "border: 1px solid #E2E2E0;"
        "font-size: 12px;"
    "}"
    "tr { border-bottom: 1px solid #E2E2E0; }"
    "th,td { padding: 5px; }"
    "th { "
        "background-color: #DCDCED;"
        "font-weight: bold;"
    "}";

OutputHtmlFile::OutputHtmlFile(std::ofstream file)
    : file_(std::move(file))
{
    // Nothing
}

// static
std::unique_ptr<OutputHtmlFile> OutputHtmlFile::Create(const FilePath& file_path)
{
    std::ofstream file;

    file.open(file_path, std::ofstream::out | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG(WARNING) << "Unable to create report file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<OutputHtmlFile>(new OutputHtmlFile(std::move(file)));
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
    meta->append_attribute(doc_.allocate_attribute("generator", "Aspia"));

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
    rapidxml::xml_node<>* h =
        doc_.allocate_node(rapidxml::node_element,
                           doc_.allocate_string(StringPrintf("h%d", h_level_).data()));

    h->value(doc_.allocate_string(name.data()));
    body_->append_node(h);

    if (h_level_ <= 6)
        ++h_level_;
}

void OutputHtmlFile::EndTableGroup()
{
    if (h_level_ > 1)
        --h_level_;
}

void OutputHtmlFile::StartTable(Category* category)
{
    DCHECK(body_);
    DCHECK(!table_);

    rapidxml::xml_node<>* h1 =
        doc_.allocate_node(rapidxml::node_element,
                           doc_.allocate_string(StringPrintf("h%d", h_level_).data()));

    h1->value(doc_.allocate_string(category->Name()));
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

void OutputHtmlFile::AddColumns(const ColumnList& column_list)
{
    DCHECK(table_);
    tr_ = doc_.allocate_node(rapidxml::node_element, "tr");

    column_count_ = 0;

    for (const auto& column : column_list)
    {
        rapidxml::xml_node<>* th = doc_.allocate_node(rapidxml::node_element, "th");
        th->value(doc_.allocate_string(column.first.data()));
        tr_->append_node(th);

        ++column_count_;
    }

    table_->append_node(tr_);
    tr_ = nullptr;
}

void OutputHtmlFile::StartGroup(std::string_view name)
{
    DCHECK(table_);

    rapidxml::xml_node<>* tr = doc_.allocate_node(rapidxml::node_element, "tr");

    for (int column_index = 0; column_index < column_count_; ++column_index)
    {
        rapidxml::xml_node<>* td = doc_.allocate_node(rapidxml::node_element, "td");

        if (column_index == 0)
        {
            std::string style = StringPrintf("font-weight: bold; padding-left: %dpx;", padding_);
            td->append_attribute(doc_.allocate_attribute("style", doc_.allocate_string(style.data())));
            td->value(doc_.allocate_string(name.data()));
        }
        else
        {
            td->value(doc_.allocate_string(" "));
        }

        tr->append_node(td);
    }

    table_->append_node(tr);

    padding_ += 12;
}

void OutputHtmlFile::EndGroup()
{
    padding_ -= 12;
}

void OutputHtmlFile::AddParam(std::string_view param, const Value& value)
{
    DCHECK(table_);

    std::string style = "font-weight: bold; padding-left: " +
        StringPrintf("%dpx;", padding_);

    rapidxml::xml_node<>* td1 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->append_attribute(doc_.allocate_attribute("style", doc_.allocate_string(style.data())));
    td1->value(doc_.allocate_string(param.data()));

    std::string value_with_unit(value.ToString());

    if (value.HasUnit())
    {
        value_with_unit.append(" ");
        value_with_unit.append(value.Unit());
    }

    rapidxml::xml_node<>* td2 = doc_.allocate_node(rapidxml::node_element, "td");
    td2->value(doc_.allocate_string(value_with_unit.data()));

    rapidxml::xml_node<>* tr = doc_.allocate_node(rapidxml::node_element, "tr");
    tr->append_node(td1);
    tr->append_node(td2);

    table_->append_node(tr);
}

void OutputHtmlFile::StartRow()
{
    DCHECK(table_);
    DCHECK(!tr_);

    tr_ = doc_.allocate_node(rapidxml::node_element, "tr");
    current_column_ = 0;
}

void OutputHtmlFile::EndRow()
{
    DCHECK(table_);
    DCHECK(tr_);

    table_->append_node(tr_);
    tr_ = nullptr;
}

void OutputHtmlFile::AddValue(const Value& value)
{
    DCHECK(table_);
    DCHECK(tr_);

    std::string value_with_unit(value.ToString());

    if (value.HasUnit())
    {
        value_with_unit.append(" ");
        value_with_unit.append(value.Unit());
    }

    rapidxml::xml_node<>* td = doc_.allocate_node(rapidxml::node_element, "td");

    if (current_column_ == 0)
    {
        std::string style = StringPrintf("padding-left: %dpx;", padding_);
        td->append_attribute(doc_.allocate_attribute("style", doc_.allocate_string(style.data())));
    }

    td->value(doc_.allocate_string(value_with_unit.data()));

    tr_->append_node(td);

    ++current_column_;
}

} // namespace aspia
