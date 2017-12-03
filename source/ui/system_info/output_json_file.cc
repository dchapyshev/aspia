//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_json_file.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/output_json_file.h"

namespace aspia {

OutputJsonFile::OutputJsonFile(std::ofstream file)
    : file_(std::move(file))
{
    // Nothing
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
    // TODO
}

void OutputJsonFile::EndDocument()
{
    // TODO
    file_.close();
}

void OutputJsonFile::StartTableGroup(std::string_view /* name */)
{
    // TODO
}

void OutputJsonFile::EndTableGroup()
{
    // TODO
}

void OutputJsonFile::StartTable(std::string_view /* name */)
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

void OutputJsonFile::AddHeaderItem(std::string_view /* name */, int /* width */)
{
    // TODO
}

void OutputJsonFile::StartGroup(std::string_view /* name */, Category::IconId /* icon_id */)
{
    // TODO
}

void OutputJsonFile::EndGroup()
{
    // TODO
}

void OutputJsonFile::AddParam(Category::IconId /* icon_id */,
                              std::string_view /* param */,
                              std::string_view /* value */,
                              std::string_view /* unit */)
{
    // TODO
}

void OutputJsonFile::StartRow(Category::IconId /* icon_id */)
{
    // TODO
}

void OutputJsonFile::EndRow()
{
    // TODO
}

void OutputJsonFile::AddValue(std::string_view /* value */, std::string_view /* unit */)
{
    // TODO
}

} // namespace aspia
