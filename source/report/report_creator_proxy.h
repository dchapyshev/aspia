//
// PROJECT:         Aspia
// FILE:            report/report_creator_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_REPORT__REPORT_CREATOR_PROXY_H
#define _ASPIA_REPORT__REPORT_CREATOR_PROXY_H

#include "report/report_creator.h"

#include <mutex>

namespace aspia {

class ReportCreatorProxy
{
public:
    bool Parse(const std::string& data);

private:
    friend class ReportCreator;

    explicit ReportCreatorProxy(ReportCreator* creator);

    // Called directly by ReportCreator::~ReportCreator.
    void WillDestroyCurrentReportCreator();

    ReportCreator* creator_;
    mutable std::mutex creator_lock_;

    DISALLOW_COPY_AND_ASSIGN(ReportCreatorProxy);
};

} // namespace aspia

#endif // _ASPIA_REPORT__REPORT_CREATOR_PROXY_H
