//
// PROJECT:         Aspia
// FILE:            console/computer_group.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_group.h"

namespace aspia {

ComputerGroup::ComputerGroup(const QIcon& icon,
                             proto::ComputerGroup* group,
                             ComputerGroup* parent_group)
    : group_(group),
      parent_group_(parent_group)
{
    setIcon(0, icon);
    setText(0, QString::fromStdString(group_->name()));

    for (int i = 0; i < group_->group_size(); ++i)
        addChild(new ComputerGroup(QIcon(QStringLiteral(":/icon/folder.png")),
                                   group_->mutable_group(i),
                                   this));
}

ComputerGroup::~ComputerGroup()
{
    if (!parent_group_)
        delete group_;
}

// static
std::unique_ptr<ComputerGroup> ComputerGroup::Create()
{
    return std::unique_ptr<ComputerGroup>(
        new ComputerGroup(QIcon(QStringLiteral(":/icon/folder.png")),
                          new proto::ComputerGroup(),
                          nullptr));
}

void ComputerGroup::AddChildComputerGroup(ComputerGroup* computer_group)
{
    group_->mutable_group()->AddAllocated(computer_group->group_);
    computer_group->parent_group_ = this;
    addChild(computer_group);
}

bool ComputerGroup::DeleteChildComputerGroup(ComputerGroup* computer_group)
{
    for (int i = 0; i < group_->group_size(); ++i)
    {
        if (group_->mutable_group(i) == computer_group->group_)
        {
            group_->mutable_group()->DeleteSubrange(i, 1);
            delete computer_group;
            return true;
        }
    }

    return false;
}

ComputerGroup* ComputerGroup::TakeChildComputerGroup(ComputerGroup* computer_group)
{
    for (int i = 0; i < childCount(); ++i)
    {
        if (child(i) == computer_group)
        {
            ComputerGroup* child_item = reinterpret_cast<ComputerGroup*>(child(i));

            for (int j = 0; j < group_->group_size(); ++j)
            {
                if (group_->mutable_group(j) == child_item->group_)
                {
                    proto::ComputerGroup* temp = new proto::ComputerGroup();
                    *temp = std::move(*group_->mutable_group(j));

                    group_->mutable_group()->DeleteSubrange(j, 1);
                    takeChild(i);

                    child_item->group_ = temp;
                    child_item->parent_group_ = nullptr;

                    return child_item;
                }
            }
        }
    }

    return nullptr;
}

void ComputerGroup::AddChildComputer(Computer* computer)
{
    group_->mutable_computer()->AddAllocated(computer->computer_);
    computer->parent_group_ = this;
}

bool ComputerGroup::DeleteChildComputer(Computer* computer)
{
    for (int i = 0; i < group_->computer_size(); ++i)
    {
        if (group_->mutable_computer(i) == computer->computer_)
        {
            group_->mutable_computer()->DeleteSubrange(i, 1);
            delete computer;
            return true;
        }
    }

    return false;
}

QString ComputerGroup::Name() const
{
    return text(0);
}

void ComputerGroup::SetName(const QString& name)
{
    group_->set_name(name.toStdString());
    setText(0, name);
}

QString ComputerGroup::Comment() const
{
    return QString::fromStdString(group_->comment());
}

void ComputerGroup::SetComment(const QString& comment)
{
    group_->set_comment(comment.toStdString());
}

bool ComputerGroup::IsExpanded() const
{
    return group_->expanded();
}

void ComputerGroup::SetExpanded(bool expanded)
{
    group_->set_expanded(expanded);
}

QList<QTreeWidgetItem*> ComputerGroup::ComputerList()
{
    QList<QTreeWidgetItem*> list;

    for (int i = 0; i < group_->computer_size(); ++i)
        list.push_back(new Computer(group_->mutable_computer(i), this));

    return list;
}

ComputerGroup* ComputerGroup::ParentComputerGroup()
{
    return parent_group_;
}

} // namespace aspia
