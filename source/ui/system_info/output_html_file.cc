//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_html_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output_html_file.h"

namespace aspia {

OutputHtmlFile::OutputHtmlFile(const std::wstring& file_path)
{
    // TODO
}

void OutputHtmlFile::StartDocument()
{
    // TODO
}

void OutputHtmlFile::EndDocument()
{
    // TODO
}

void OutputHtmlFile::StartTable(const std::string& name)
{
    // TODO
}

void OutputHtmlFile::EndTable()
{
    // TODO
}

void OutputHtmlFile::StartTableHeader()
{
    // TODO
}

void OutputHtmlFile::EndTableHeader()
{
    // TODO
}

void OutputHtmlFile::AddHeaderItem(const std::string& name, int width)
{
    // TODO
}

void OutputHtmlFile::StartGroup(const std::string& name, Category::IconId icon_id)
{
    // TODO
}

void OutputHtmlFile::EndGroup()
{
    // TODO
}

void OutputHtmlFile::AddParam(Category::IconId icon_id,
                              const std::string& param,
                              const std::string& value,
                              const char* unit)
{
    // TODO
}

void OutputHtmlFile::StartRow(Category::IconId icon_id)
{
    // TODO
}

void OutputHtmlFile::EndRow()
{
    // TODO
}

void OutputHtmlFile::AddValue(const std::string& value, const char* unit)
{
    // TODO
}

} // namespace aspia
