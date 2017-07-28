//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_request_sender.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_request_sender.h"
#include "client/file_request_sender_proxy.h"

namespace aspia {

FileRequestSender::FileRequestSender()
{
    sender_proxy_.reset(new FileRequestSenderProxy(this));
}

FileRequestSender::~FileRequestSender()
{
    sender_proxy_->WillDestroyCurrentRequestSender();
    sender_proxy_ = nullptr;
}

std::shared_ptr<FileRequestSenderProxy>
FileRequestSender::request_sender_proxy() const
{
    return sender_proxy_;
}

} // namespace aspia
