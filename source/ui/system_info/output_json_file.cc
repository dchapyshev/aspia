//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_json_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output_json_file.h"

namespace aspia {

OutputJsonFile::OutputJsonFile(const std::wstring& file_path)
{
    // TODO
}

void OutputJsonFile::StartDocument()
{
    // TODO
}

void OutputJsonFile::EndDocument()
{
    // TODO
}

void OutputJsonFile::StartTable(const std::string& name)
{
    // TODO
}

void OutputJsonFile::EndTable()
{
    // TODO
}

void OutputJsonFile::StartTableHeader()
{
    // TODO
}

void OutputJsonFile::EndTableHeader()
{
    // TODO
}

void OutputJsonFile::AddHeaderItem(const std::string& name, int width)
{
    // TODO
}

void OutputJsonFile::StartGroup(const std::string& name, Category::IconId icon_id)
{
    // TODO
}

void OutputJsonFile::EndGroup()
{
    // TODO
}

void OutputJsonFile::AddParam(Category::IconId icon_id,
                              const std::string& param,
                              const std::string& value,
                              const char* unit)
{
    // TODO
}

void OutputJsonFile::StartRow(Category::IconId icon_id)
{
    // TODO
}

void OutputJsonFile::EndRow()
{
    // TODO
}

void OutputJsonFile::AddValue(const std::string& value, const char* unit)
{
    // TODO
}

} // namespace aspia
