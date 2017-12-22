//
// PROJECT:         Aspia
// FILE:            category/category_task_scheduler.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "category/category_task_scheduler.h"
#include "category/category_task_scheduler.pb.h"
#include "ui/resource.h"

namespace aspia {

CategoryTaskScheduler::CategoryTaskScheduler()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryTaskScheduler::Name() const
{
    return "Task Scheduler";
}

Category::IconId CategoryTaskScheduler::Icon() const
{
    return IDI_CLOCK;
}

const char* CategoryTaskScheduler::Guid() const
{
    return "{1B27C27F-847E-47CC-92DF-6B8F5CB4827A}";
}

void CategoryTaskScheduler::Parse(Table& /* table */, const std::string& /* data */)
{
    // TODO
}

std::string CategoryTaskScheduler::Serialize()
{
    // TODO
    return std::string();
}

} // namespace aspia
