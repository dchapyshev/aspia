//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/sys_info/sys_info_widget_local_users.h"

#include <QMenu>

#include "common/system_info_constants.h"

namespace client {

namespace {

class Item : public QTreeWidgetItem
{
public:
    Item(const char* icon_path, const QString& text, const QList<QTreeWidgetItem*>& childs)
    {
        QIcon icon(icon_path);

        setIcon(0, icon);
        setText(0, text);

        for (const auto& child : childs)
        {
            child->setIcon(0, icon);

            for (int i = 0; i < child->childCount(); ++i)
            {
                QTreeWidgetItem* item = child->child(i);
                if (item)
                    item->setIcon(0, icon);
            }
        }

        addChildren(childs);
    }

    Item(const QString& text, const QList<QTreeWidgetItem*>& params)
    {
        setText(0, text);
        addChildren(params);
    }

private:
    Q_DISABLE_COPY(Item)
};

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const QString& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();

    item->setText(0, param);
    item->setText(1, value);

    return item;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* mk(const QString& param, const std::string& value)
{
    return mk(param, QString::fromStdString(value));
}

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetLocalUsers::SysInfoWidgetLocalUsers(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_name, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 0);
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), 1);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetLocalUsers::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetLocalUsers::~SysInfoWidgetLocalUsers() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetLocalUsers::category() const
{
    return common::kSystemInfo_LocalUsers;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetLocalUsers::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();

    if (!system_info.has_local_users())
    {
        ui.tree->setEnabled(false);
        return;
    }

    const proto::system_info::LocalUsers& users = system_info.local_users();

    for (int i = 0; i < users.local_user_size(); ++i)
    {
        const proto::system_info::LocalUsers::LocalUser& user = users.local_user(i);
        QList<QTreeWidgetItem*> group;

        if (user.name().empty())
            continue;

        if (!user.full_name().empty())
            group << mk(tr("Full Name"), user.full_name());

        if (!user.comment().empty())
            group << mk(tr("Description"), user.comment());

        if (!user.home_dir().empty())
            group << mk(tr("Home Directory"), user.home_dir());

        group << mk(tr("Disabled"), user.disabled() ? tr("Yes") : tr("No"));
        group << mk(tr("Password Can't Change"), user.password_cant_change() ? tr("Yes") : tr("No"));
        group << mk(tr("Password Expired"), user.password_expired() ? tr("Yes") : tr("No"));
        group << mk(tr("Don't Expire Password"), user.dont_expire_password() ? tr("Yes") : tr("No"));
        group << mk(tr("Lockout"), user.lockout() ? tr("Yes") : tr("No"));

        QString last_logon;
        if (user.last_logon_time() == 0)
            last_logon = tr("Never");
        else
            last_logon = timeToString(static_cast<time_t>(user.last_logon_time()));

        group << mk(tr("Last Logon"), last_logon);
        group << mk(tr("Number Logons"), QString::number(user.number_logons()));
        group << mk(tr("Bad Password Count"), QString::number(user.bad_password_count()));

        QList<QTreeWidgetItem*> groups_group;

        for (int j = 0; j < user.group_size(); ++j)
        {
            const proto::system_info::LocalUsers::LocalUser::LocalUserGroup& local_user_group =
                user.group(j);
            groups_group << mk(QString::fromStdString(local_user_group.name()), local_user_group.comment());
        }

        if (!groups_group.isEmpty())
            group << new Item(tr("Groups"), groups_group);

        if (!group.isEmpty())
        {
            ui.tree->addTopLevelItem(
                new Item(user.disabled() ? ":/img/user-disabled.png" : ":/img/user.png",
                         QString::fromStdString(user.name()), group));
        }
    }

    ui.tree->expandAll();
    ui.tree->resizeColumnToContents(0);
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetLocalUsers::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetLocalUsers::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_name);
    menu.addAction(ui.action_copy_value);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

} // namespace client
