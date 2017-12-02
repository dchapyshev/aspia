//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/document_creater.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H
#define _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H

#include "ui/system_info/output.h"

#include <functional>
#include <stack>

namespace aspia {

class DocumentCreaterProxy;

class DocumentCreater
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void OnRequest(const std::string& guid,
                               std::shared_ptr<DocumentCreaterProxy> creater) = 0;
    };

    DocumentCreater(CategoryList* list, Output* output, Delegate* delegate);
    ~DocumentCreater();

    enum class State { REQUEST, OUTPUT };
    using StateChangeCallback =
        std::function<void(std::string_view category_name, State state)>;
    using TerminateCallback = std::function<void()>;

    void Start(StateChangeCallback state_change_callback,
               TerminateCallback terminate_callback);

private:
    friend class DocumentCreaterProxy;

    void Parse(std::shared_ptr<std::string> data);

    void ProcessNextItem();
    static bool HasCheckedItems(CategoryGroup* parent_group);

    std::shared_ptr<DocumentCreaterProxy> proxy_;

    Delegate* delegate_;

    StateChangeCallback state_change_callback_;
    TerminateCallback terminate_callback_;
    CategoryList* list_;
    Output* output_;

    std::stack<std::pair<CategoryList*, CategoryList::iterator>> iterator_stack_;

    DISALLOW_COPY_AND_ASSIGN(DocumentCreater);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H
