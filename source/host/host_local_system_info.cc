//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_local_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "host/host_local_system_info.h"
#include "protocol/category.h"
#include "ui/system_info/document_creater_proxy.h"

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

    window_.reset(new SystemInfoWindow(this, this));
}

void HostLocalSystemInfo::OnAfterThreadRunning()
{
    window_.reset();
}

void HostLocalSystemInfo::OnRequest(const std::string& guid,
                                    std::shared_ptr<DocumentCreaterProxy> creater)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&HostLocalSystemInfo::OnRequest, this, guid, creater));
        return;
    }

    // Looking for a category by GUID.
    const auto category = map_.find(guid);
    if (category != map_.end())
    {
        std::shared_ptr<std::string> data =
            std::make_shared<std::string>(category->second->Serialize());

        creater->Parse(std::move(data));
    }
}

void HostLocalSystemInfo::OnWindowClose()
{
    runner_->PostQuit();
}

} // namespace aspia
