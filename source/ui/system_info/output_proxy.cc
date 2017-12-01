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

bool OutputProxy::StartDocument()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartDocument();
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

bool OutputProxy::StartTableGroup(std::string_view name)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartTableGroup(name);
    return true;
}

bool OutputProxy::EndTableGroup()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndTableGroup();
    return true;
}

bool OutputProxy::StartTable(std::string_view name)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartTable(name);
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

bool OutputProxy::StartTableHeader()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->StartTableHeader();
    return true;
}

bool OutputProxy::EndTableHeader()
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->EndTableHeader();
    return true;
}

bool OutputProxy::AddHeaderItem(std::string_view name, int width)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->AddHeaderItem(name, width);
    return true;
}

bool OutputProxy::StartGroup(std::string_view name, Category::IconId icon_id)
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
                           std::string_view param,
                           std::string_view value,
                           std::string_view unit)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->AddParam(icon_id, param, value, unit);
    return true;
}

bool OutputProxy::AddParam(Category::IconId icon_id,
                           std::string_view param,
                           std::string_view value)
{
    return AddParam(icon_id, param, value, std::string());
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

bool OutputProxy::AddValue(std::string_view value, std::string_view unit)
{
    std::lock_guard<std::mutex> lock(output_lock_);

    if (!output_)
        return false;

    output_->AddValue(value, unit);
    return true;
}

bool OutputProxy::AddValue(std::string_view value)
{
    return AddValue(value, std::string());
}

} // namespace aspia
