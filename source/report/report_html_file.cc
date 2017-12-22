//
// PROJECT:         Aspia
// FILE:            report/report_html_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/logging.h"
#include "report/report_html_file.h"

#pragma warning(push, 3)
#include <rapidxml_print.hpp>
#pragma warning(pop)

namespace aspia {

namespace {
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

std::string ValueToString(const Value& value)
{
    switch (value.type())
    {
        case Value::Type::BOOL:
            return value.ToBool() ? "Yes" : "No";

        case Value::Type::STRING:
            return value.ToString().data();

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

} // namespace

ReportHtmlFile::ReportHtmlFile(std::ofstream file)
    : file_(std::move(file))
{
    // Nothing
}

// static
std::unique_ptr<ReportHtmlFile> ReportHtmlFile::Create(const FilePath& file_path)
{
    std::ofstream file;

    file.open(file_path, std::ofstream::out | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG(WARNING) << "Unable to create report file: " << file_path;
        return nullptr;
    }

    return std::unique_ptr<ReportHtmlFile>(new ReportHtmlFile(std::move(file)));
}

void ReportHtmlFile::StartDocument()
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

void ReportHtmlFile::EndDocument()
{
    DCHECK(html_);
    DCHECK(body_);

    html_->append_node(body_);
    doc_.append_node(html_);

    file_ << doc_;
    file_.close();
    doc_.clear();
}

void ReportHtmlFile::StartTableGroup(std::string_view name)
{
    rapidxml::xml_node<>* h =
        doc_.allocate_node(rapidxml::node_element,
                           doc_.allocate_string(StringPrintf("h%d", h_level_).data()));

    h->value(doc_.allocate_string(name.data()));
    body_->append_node(h);

    if (h_level_ <= 6)
        ++h_level_;
}

void ReportHtmlFile::EndTableGroup()
{
    if (h_level_ > 1)
        --h_level_;
}

void ReportHtmlFile::StartTable(Category* category)
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

void ReportHtmlFile::EndTable()
{
    DCHECK(body_);
    DCHECK(table_);

    body_->append_node(table_);
    table_ = nullptr;
}

void ReportHtmlFile::AddColumns(const ColumnList& column_list)
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

void ReportHtmlFile::StartGroup(std::string_view name)
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

void ReportHtmlFile::EndGroup()
{
    padding_ -= 12;
}

void ReportHtmlFile::AddParam(std::string_view param, const Value& value)
{
    DCHECK(table_);

    std::string style = "font-weight: bold; padding-left: " +
        StringPrintf("%dpx;", padding_);

    rapidxml::xml_node<>* td1 = doc_.allocate_node(rapidxml::node_element, "td");
    td1->append_attribute(doc_.allocate_attribute("style", doc_.allocate_string(style.data())));
    td1->value(doc_.allocate_string(param.data()));

    std::string value_with_unit = ValueToString(value);

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

void ReportHtmlFile::StartRow()
{
    DCHECK(table_);
    DCHECK(!tr_);

    tr_ = doc_.allocate_node(rapidxml::node_element, "tr");
    current_column_ = 0;
}

void ReportHtmlFile::EndRow()
{
    DCHECK(table_);
    DCHECK(tr_);

    table_->append_node(tr_);
    tr_ = nullptr;
}

void ReportHtmlFile::AddValue(const Value& value)
{
    DCHECK(table_);
    DCHECK(tr_);

    std::string value_with_unit = ValueToString(value);

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
