//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_local_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "host/host_local_system_info.h"
#include "protocol/category.h"
#include "ui/system_info/report_creator_proxy.h"

namespace aspia {

static const size_t kGuidLength = 38;

void HostLocalSystemInfo::Run()
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
    thread_.Join();
}

void HostLocalSystemInfo::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);

    map_ = CreateCategoryMap();

    window_.reset(new SystemInfoWindow(this));
}

void HostLocalSystemInfo::OnAfterThreadRunning()
{
    window_.reset();
}

void HostLocalSystemInfo::OnRequest(std::string_view guid,
                                    std::shared_ptr<ReportCreatorProxy> report_creator)
{
    runner_->PostTask(std::bind(&HostLocalSystemInfo::ExecuteRequest, this, guid, report_creator));
}

void HostLocalSystemInfo::OnWindowClose()
{
    runner_->PostQuit();
}

void HostLocalSystemInfo::ExecuteRequest(std::string_view guid,
                                         std::shared_ptr<ReportCreatorProxy> report_creator)
{
    DCHECK(guid.length() == kGuidLength && guid.front() == '{' && guid.back() == '}');
    DCHECK(runner_->BelongsToCurrentThread());
    DCHECK(report_creator != nullptr);

    std::string data;

    // Looking for a category by GUID.
    const auto& category = map_.find(guid.data());
    if (category != map_.end())
        data = category->second->Serialize();

    report_creator->Parse(data);
}

} // namespace aspia
