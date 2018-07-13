//
// PROJECT:         Aspia
// FILE:            client/ui/system_info_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/system_info_window.h"

#include <QLineEdit>
#include <QMenu>

#include "client/ui/category_group_tree_item.h"
#include "client/ui/category_tree_item.h"
#include "host/system_info_request.h"
#include "system_info/category.h"

namespace aspia {

SystemInfoWindow::SystemInfoWindow(ConnectData* connect_data, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    QString computer_name;
    if (!connect_data->computerName().isEmpty())
        computer_name = connect_data->computerName();
    else
        computer_name = connect_data->address();

    setWindowTitle(tr("%1 - Aspia System Information").arg(computer_name));

    menu_ = new QMenu;
    menu_->addAction(ui.action_save_current);
    menu_->addAction(ui.action_save_selected);
    menu_->addAction(ui.action_save_all);

    ui.action_save_report->setMenu(menu_);

    search_edit_ = new QLineEdit(ui.toolbar);
    ui.toolbar->addWidget(search_edit_);
}

void SystemInfoWindow::refresh()
{
    if (!ui.category_tree->topLevelItemCount())
    {
        SystemInfoRequest* request = SystemInfoRequest::categoryList();

        connect(request, &SystemInfoRequest::replyReady,
                this, &SystemInfoWindow::onCategoryListReceived);

        emit newRequest(request);
    }
}

void SystemInfoWindow::closeEvent(QCloseEvent* event)
{
    emit windowClose();
    QWidget::closeEvent(event);
}

void SystemInfoWindow::onCategoryListReceived(const proto::system_info::Reply& reply)
{
    const proto::system_info::CategoryList& category_list = reply.category_list();

    QSet<QString> supported_categories;
    supported_categories.reserve(category_list.uuid_size());

    for (int i = 0; i < category_list.uuid_size(); ++i)
        supported_categories.insert(QString::fromStdString(category_list.uuid(i)));

    std::function<QList<QTreeWidgetItem*>(const CategoryGroup&)> create_children =
        [&](const CategoryGroup& parent_group)
    {
        QList<QTreeWidgetItem*> items;

        for (const auto& group : parent_group.childGroupList())
        {
            QList<QTreeWidgetItem*> children = create_children(group);
            if (!children.isEmpty())
            {
                CategoryGroupTreeItem* group_tree_item = new CategoryGroupTreeItem(group);
                group_tree_item->addChildren(children);
                items.append(group_tree_item);
            }
        }

        for (const auto& category : parent_group.childCategoryList())
        {
            if (supported_categories.contains(category.uuid()))
                items.append(new CategoryTreeItem(category));
        }

        return items;
    };

    QList<QTreeWidgetItem*> items;

    for (const auto& group : CategoryGroup::rootGroups())
    {
        QList<QTreeWidgetItem*> children = create_children(group);
        if (!children.isEmpty())
        {
            CategoryGroupTreeItem* group_tree_item = new CategoryGroupTreeItem(group);
            group_tree_item->addChildren(children);
            items.append(group_tree_item);
        }
    }

    if (!items.isEmpty())
        ui.category_tree->addTopLevelItems(items);

    ui.category_tree->expandAll();
}

} // namespace aspia
