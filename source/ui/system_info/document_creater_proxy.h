//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/document_creater_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_PROXY_H
#define _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_PROXY_H

#include "ui/system_info/document_creater.h"

#include <mutex>

namespace aspia {

class DocumentCreaterProxy
{
public:
    bool Parse(std::shared_ptr<std::string> data);

private:
    friend class DocumentCreater;

    explicit DocumentCreaterProxy(DocumentCreater* creater);

    // Called directly by DocumentCreater::~DocumentCreater.
    void WillDestroyCurrentDocumentCreater();

    DocumentCreater* creater_;
    mutable std::mutex creater_lock_;

    DISALLOW_COPY_AND_ASSIGN(DocumentCreaterProxy);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_PROXY_H
