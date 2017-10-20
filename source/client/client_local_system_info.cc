//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_local_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "client/client_local_system_info.h"

namespace aspia {

ClientLocalSystemInfo::ClientLocalSystemInfo(Delegate* delegate)
    : delegate_(delegate)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

ClientLocalSystemInfo::~ClientLocalSystemInfo()
{
    thread_.Stop();
}

void ClientLocalSystemInfo::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);

    map_ = CreateCategoryMap();
    window_.reset(new SystemInfoWindow(this));
}

void ClientLocalSystemInfo::OnAfterThreadRunning()
{
    delegate_->OnClose();
    window_.reset();
}

void ClientLocalSystemInfo::OnRequest(GuidList list, std::shared_ptr<OutputProxy> output)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ClientLocalSystemInfo::OnRequest, this, list, output));
        return;
    }

    guid_list_ = std::move(list);
    output_ = std::move(output);

    output_->StartDocument("System Information");

    SendRequest();
}

void ClientLocalSystemInfo::SendRequest()
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
            runner_->PostTask(std::bind(&ClientLocalSystemInfo::SendRequest, this));
        }
    }
}

void ClientLocalSystemInfo::OnWindowClose()
{
    terminated_ = true;
    runner_->PostQuit();
}

bool ClientLocalSystemInfo::IsTerminated() const
{
    return terminated_;
}

} // namespace aspia
