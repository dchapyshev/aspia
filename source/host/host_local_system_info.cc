//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_local_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "host/host_local_system_info.h"

namespace aspia {

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

void HostLocalSystemInfo::OnRequest(GuidList list, std::shared_ptr<OutputProxy> output)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostLocalSystemInfo::OnRequest, this, list, output));
        return;
    }

    guid_list_ = std::move(list);
    output_ = std::move(output);

    output_->StartDocument();

    SendRequest();
}

void HostLocalSystemInfo::SendRequest()
{
    // Looking for a category by GUID.
    const auto category = map_.find(guid_list_.front());
    if (category != map_.end())
    {
        category->second->Parse(output_, category->second->Serialize());
        guid_list_.pop_front();

        if (guid_list_.empty())
        {
            output_->EndDocument();
        }
        else
        {
            runner_->PostTask(std::bind(&HostLocalSystemInfo::SendRequest, this));
        }
    }
}

void HostLocalSystemInfo::OnWindowClose()
{
    runner_->PostQuit();
}

} // namespace aspia
