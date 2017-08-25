//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/category_group_event_log.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_EVENT_LOG_H
#define _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_EVENT_LOG_H

#include "ui/system_info/category_group.h"

namespace aspia {

class CategoryGroupEventLog : public CategoryGroup
{
public:
    CategoryGroupEventLog();

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryGroupEventLog);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__CATEGORY_GROUP_EVENT_LOG_H
