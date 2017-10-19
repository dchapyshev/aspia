//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/output_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/output_proxy.h"

namespace aspia {

OutputProxy::OutputProxy(Output* output)
    : output_(output)
{
    // Nothing
}

void OutputProxy::WillDestroyCurrentOutput()
{
    std::lock_guard<std::mutex> lock(output_lock_);
    output_ = nullptr;
}

bool OutputProxy::StartDocument(const std::string& name)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartDocument(name);
    return true;
}

bool OutputProxy::EndDocument()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndDocument();
    return true;
}

bool OutputProxy::StartTable(const std::string& name, const ColumnList& column_list)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartTable(name, column_list);
    return true;
}

bool OutputProxy::EndTable()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndTable();
    return true;
}

bool OutputProxy::StartGroup(const std::string& name, Category::IconId icon_id)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartGroup(name, icon_id);
    return true;
}

bool OutputProxy::EndGroup()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndGroup();
    return true;
}

bool OutputProxy::AddParam(Category::IconId icon_id,
                           const std::string& param,
                           const std::string& value,
                           const char* unit)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->AddParam(icon_id, param, value, unit);
    return true;
}

bool OutputProxy::StartRow(Category::IconId icon_id)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartRow(icon_id);
    return true;
}

bool OutputProxy::EndRow()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndRow();
    return true;
}

bool OutputProxy::AddValue(const std::string& value, const char* unit)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->AddValue(value, unit);
    return true;
}

} // namespace aspia
