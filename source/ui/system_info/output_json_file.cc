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
    UNUSED_PARAMETER(file_path);
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

void OutputJsonFile::StartTableGroup(std::string_view name)
{
    UNUSED_PARAMETER(name);
    // TODO
}

void OutputJsonFile::EndTableGroup()
{
    // TODO
}

void OutputJsonFile::StartTable(std::string_view name)
{
    UNUSED_PARAMETER(name);
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

void OutputJsonFile::AddHeaderItem(std::string_view name, int width)
{
    UNUSED_PARAMETER(name);
    UNUSED_PARAMETER(width);
    // TODO
}

void OutputJsonFile::StartGroup(std::string_view name, Category::IconId icon_id)
{
    UNUSED_PARAMETER(name);
    UNUSED_PARAMETER(icon_id);
    // TODO
}

void OutputJsonFile::EndGroup()
{
    // TODO
}

void OutputJsonFile::AddParam(Category::IconId icon_id,
                              std::string_view param,
                              std::string_view value,
                              std::string_view unit)
{
    UNUSED_PARAMETER(icon_id);
    UNUSED_PARAMETER(param);
    UNUSED_PARAMETER(value);
    UNUSED_PARAMETER(unit);
    // TODO
}

void OutputJsonFile::StartRow(Category::IconId icon_id)
{
    UNUSED_PARAMETER(icon_id);
    // TODO
}

void OutputJsonFile::EndRow()
{
    // TODO
}

void OutputJsonFile::AddValue(std::string_view value, std::string_view unit)
{
    UNUSED_PARAMETER(value);
    UNUSED_PARAMETER(unit);
    // TODO
}

} // namespace aspia
