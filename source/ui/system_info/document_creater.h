//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/document_creater.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H
#define _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H

#include "base/message_loop/message_loop_thread.h"
#include "ui/system_info/output_proxy.h"

#include <stack>

namespace aspia {

class DocumentCreaterProxy;

class DocumentCreater
    : private MessageLoopThread::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void OnRequest(const std::string& guid,
                               std::shared_ptr<DocumentCreaterProxy> creater) = 0;
    };

    DocumentCreater(Delegate* delegate, std::shared_ptr<OutputProxy> output);
    ~DocumentCreater();

    std::shared_ptr<DocumentCreaterProxy> document_creater_proxy() const { return proxy_; }

    void CreateDocument();

    const CategoryList& tree() const { return tree_; }
    CategoryList* mutable_tree() { return &tree_; }

private:
    friend class DocumentCreaterProxy;

    void Parse(std::shared_ptr<std::string> data);

    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void ProcessNextItem();
    static bool HasCheckedItems(CategoryGroup* parent_group);

    std::shared_ptr<DocumentCreaterProxy> proxy_;

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;
    std::shared_ptr<OutputProxy> output_;
    CategoryList tree_;
    std::stack<std::pair<CategoryList*, CategoryList::iterator>> iterator_stack_;

    DISALLOW_COPY_AND_ASSIGN(DocumentCreater);
};

} // namespace aspia

#endif // _ASPIA_UI__SYSTEM_INFO__DOCUMENT_CREATER_H
