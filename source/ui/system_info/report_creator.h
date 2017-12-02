//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/report_creator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__REPORT_CREATOR_H
#define _ASPIA_UI__SYSTEM_INFO__REPORT_CREATOR_H

#include "ui/system_info/output.h"

#include <functional>
#include <stack>

namespace aspia {

class ReportCreatorProxy;

class ReportCreator
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void OnRequest(const std::string& guid,
                               std::shared_ptr<ReportCreatorProxy> creater) = 0;
    };

    ReportCreator(CategoryList* list, Output* output, Delegate* delegate);
    ~ReportCreator();

    enum class State { REQUEST, OUTPUT };
    using StateChangeCallback =
        std::function<void(std::string_view category_name, State state)>;
    using TerminateCallback = std::function<void()>;

    void Start(StateChangeCallback state_change_callback,
               TerminateCallback terminate_callback);

private:
    friend class ReportCreatorProxy;

    void Parse(std::shared_ptr<std::string> data);

    void ProcessNextItem();
    static bool HasCheckedItems(CategoryGroup* parent_group);

    std::shared_ptr<ReportCreatorProxy> proxy_;

    Delegate* delegate_;

    StateChangeCallback state_change_callback_;
    TerminateCallback terminate_callback_;
    CategoryList* list_;
    Output* output_;

    std::stack<std::pair<CategoryList*, CategoryList::iterator>> iterator_stack_;

    DISALLOW_COPY_AND_ASSIGN(ReportCreator);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__REPORT_CREATOR_H
