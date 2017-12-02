//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/report_creator_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/report_creator_proxy.h"

namespace aspia {

ReportCreatorProxy::ReportCreatorProxy(ReportCreator* creator)
    : creator_(creator)
{
    // Nothing
}

void ReportCreatorProxy::WillDestroyCurrentReportCreator()
{
    std::lock_guard<std::mutex> lock(creator_lock_);
    creator_ = nullptr;
}

bool ReportCreatorProxy::Parse(std::shared_ptr<std::string> data)
{
    std::lock_guard<std::mutex> lock(creator_lock_);

    if (!creator_)
        return false;

    creator_->Parse(std::move(data));
    return true;
}

} // namespace aspia
