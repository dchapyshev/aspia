//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/system_info/document_creater.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/logging.h"
#include "ui/system_info/document_creater.h"
#include "ui/system_info/document_creater_proxy.h"

namespace aspia {

DocumentCreater::DocumentCreater(Delegate* delegate, std::shared_ptr<OutputProxy> output)
    : delegate_(delegate),
      output_(std::move(output)),
      tree_(CreateCategoryTree())
{
    DCHECK(delegate_);
    DCHECK(output_);

    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

DocumentCreater::~DocumentCreater()
{
    thread_.Stop();
}

void DocumentCreater::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);

    proxy_.reset(new DocumentCreaterProxy(this));
}

void DocumentCreater::OnAfterThreadRunning()
{
    proxy_->WillDestroyCurrentDocumentCreater();
    proxy_ = nullptr;
}

void DocumentCreater::CreateDocument()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DocumentCreater::CreateDocument, this));
        return;
    }

    ProcessNextItem();
}

void DocumentCreater::ProcessNextItem()
{
    if (iterator_stack_.empty())
    {
        output_->StartDocument();
        iterator_stack_.emplace(&tree_, tree_.begin());
    }

    CategoryList* list = iterator_stack_.top().first;
    CategoryList::iterator& iterator = iterator_stack_.top().second;

    if (iterator == list->end())
    {
        iterator_stack_.pop();

        if (iterator_stack_.empty())
        {
            output_->EndDocument();
            return;
        }

        output_->EndTableGroup();
    }
    else
    {
        Category* category = iterator->get();
        DCHECK(category);

        if (category->type() == Category::Type::INFO)
        {
            CategoryInfo* category_info = category->category_info();

            if (category_info->IsChecked())
            {
                delegate_->OnRequest(category_info->Guid(), document_creater_proxy());
                return;
            }
        }
        else
        {
            DCHECK(category->type() == Category::Type::GROUP);

            CategoryGroup* category_group = category->category_group();

            if (HasCheckedItems(category_group))
            {
                output_->StartTableGroup(category_group->Name());

                iterator_stack_.emplace(category_group->mutable_child_list(),
                                        category_group->mutable_child_list()->begin());
            }
        }

        ++iterator;
    }

    ProcessNextItem();
}

// static
bool DocumentCreater::HasCheckedItems(CategoryGroup* parent_group)
{
    const CategoryList& child_list = parent_group->child_list();

    for (CategoryList::const_iterator iter = child_list.cbegin();
         iter != child_list.end();
         ++iter)
    {
        Category* category = iter->get();

        if (category->type() == Category::Type::INFO)
        {
            if (category->category_info()->IsChecked())
                return true;
        }
        else
        {
            DCHECK(category->type() == Category::Type::GROUP);

            if (HasCheckedItems(category->category_group()))
                return true;
        }
    }

    return false;
}

void DocumentCreater::Parse(std::shared_ptr<std::string> data)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&DocumentCreater::Parse, this, data));
        return;
    }

    CategoryList::iterator& iterator = iterator_stack_.top().second;

    Category* category = iterator->get();
    DCHECK(category);
    DCHECK(category->type() == Category::Type::INFO);

    category->category_info()->Parse(output_, *data);
    category->category_info()->SetChecked(false);

    ++iterator;

    ProcessNextItem();
}

} // namespace aspia
