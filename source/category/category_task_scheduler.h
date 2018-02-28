//
// PROJECT:         Aspia
// FILE:            category/category_task_scheduler.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CATEGORY__CATEGORY_TASK_SCHEDULER_H
#define _ASPIA_CATEGORY__CATEGORY_TASK_SCHEDULER_H

#include "category/category.h"

namespace aspia {

class CategoryTaskScheduler : public CategoryInfo
{
public:
    CategoryTaskScheduler();

    const char* Name() const final;
    IconId Icon() const final;

    const char* Guid() const final;
    void Parse(Table& table, const std::string& data) final;
    std::string Serialize() final;

private:
    DISALLOW_COPY_AND_ASSIGN(CategoryTaskScheduler);
};

} // namespace aspia

#endif // _ASPIA_CATEGORY__CATEGORY_TASK_SCHEDULER_H
