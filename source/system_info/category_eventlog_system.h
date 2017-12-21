//
// PROJECT:         Aspia
// FILE:            system_info/category_eventlog_system.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_SYSTEM_INFO__CATEGORY_EVENTLOG_SYSTEM_H
#define _ASPIA_SYSTEM_INFO__CATEGORY_EVENTLOG_SYSTEM_H

#include "system_info/category.h"

namespace aspia {

class CategoryEventLogSystem : public CategoryInfo
{
public:
    CategoryEventLogSystem();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryEventLogSystem);
};

} // namespace aspia

#endif // _ASPIA_SYSTEM_INFO__CATEGORY_EVENTLOG_SYSTEM_H
