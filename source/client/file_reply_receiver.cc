//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/file_reply_receiver.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_reply_receiver.h"
#include "client/file_reply_receiver_proxy.h"

namespace aspia {

FileReplyReceiver::FileReplyReceiver()
{
    receiver_proxy_.reset(new FileReplyReceiverProxy(this));
}

FileReplyReceiver::~FileReplyReceiver()
{
    receiver_proxy_->WillDestroyCurrentReplyReceiver();
    receiver_proxy_ = nullptr;
}

std::shared_ptr<FileReplyReceiverProxy> FileReplyReceiver::This()
{
    return receiver_proxy_;
}

} // namespace aspia
