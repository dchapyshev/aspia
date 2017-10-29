//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/document_creater_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/document_creater_proxy.h"

namespace aspia {

DocumentCreaterProxy::DocumentCreaterProxy(DocumentCreater* creater)
    : creater_(creater)
{
    // Nothing
}

void DocumentCreaterProxy::WillDestroyCurrentDocumentCreater()
{
    std::lock_guard<std::mutex> lock(creater_lock_);
    creater_ = nullptr;
}

bool DocumentCreaterProxy::Parse(std::shared_ptr<std::string> data)
{
    std::lock_guard<std::mutex> lock(creater_lock_);

    if (!creater_)
        return false;

    creater_->Parse(std::move(data));
    return true;
}

} // namespace aspia
