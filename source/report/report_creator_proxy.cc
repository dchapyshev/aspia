//
// PROJECT:         Aspia
// FILE:            report/report_creator_proxy.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "report/report_creator_proxy.h"

namespace aspia {

ReportCreatorProxy::ReportCreatorProxy(ReportCreator* creator)
    : creator_(creator)
{
    // Nothing
}

void ReportCreatorProxy::WillDestroyCurrentReportCreator()
{
    std::scoped_lock<std::mutex> lock(creator_lock_);
    creator_ = nullptr;
}

bool ReportCreatorProxy::Parse(const std::string& data)
{
    std::scoped_lock<std::mutex> lock(creator_lock_);

    if (!creator_)
        return false;

    creator_->Parse(data);
    return true;
}

} // namespace aspia
