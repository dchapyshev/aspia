//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/android/profiles_widget.h"

#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/config.h"
#include "client/database.h"
#include "client/android/profile_editor.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kProfileIdRole = Qt::UserRole + 1;

} // namespace

//--------------------------------------------------------------------------------------------------
ProfilesWidget::ProfilesWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      tree_(new TreeWidget()),
      editor_(new ProfileEditor(this)),
      button_add_(new IconButton(":/img/material/add_2.svg", this))
{
    button_add_->hide();

    tree_->setRootIsDecorated(false);
    tree_->setColumnCount(1);

    QWidget* list_page = new QWidget(stack_);
    QVBoxLayout* list_layout = new QVBoxLayout(list_page);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->setSpacing(0);
    list_layout->addWidget(tree_);

    stack_->addWidget(list_page);
    stack_->addWidget(editor_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(button_add_, &IconButton::clicked, this, &ProfilesWidget::onAddProfile);
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        onItemClicked(item);
    });
    connect(editor_, &ProfileEditor::sig_accepted, this, &ProfilesWidget::onReturnFromEditor);
}

//--------------------------------------------------------------------------------------------------
ProfilesWidget::~ProfilesWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> ProfilesWidget::appBarActions() const
{
    if (!isListPage())
        return {};

    return { button_add_ };
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::reload()
{
    tree_->clear();

    const QIcon icon = GuiApplication::svgIcon(":/img/dashboard-layout.svg");

    QList<ProfileConfig> profiles;
    const Database::ReadResult result = Database::instance().profileList(&profiles);
    if (result != Database::ReadResult::OK)
        LOG(ERROR) << "Unable to read the list of profiles:" << result;

    for (const ProfileConfig& profile : std::as_const(profiles))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(
            tree_, { profile.isDefault() ? tr("Default") : profile.name() });
        item->setIcon(0, icon);
        item->setData(0, kProfileIdRole, profile.isDefault() ? qint64(0) : profile.id());
    }
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::goBack()
{
    showList();
}

//--------------------------------------------------------------------------------------------------
bool ProfilesWidget::isListPage() const
{
    return stack_->currentIndex() == 0;
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::onAddProfile()
{
    editor_->prepareForAdd();
    stack_->setCurrentWidget(editor_);
    emit sig_titleChanged(tr("Add Profile"));
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::onItemClicked(QTreeWidgetItem* item)
{
    if (!item || !editor_->prepareForEdit(item->data(0, kProfileIdRole).toLongLong()))
        return;

    stack_->setCurrentWidget(editor_);
    emit sig_titleChanged(tr("Edit Profile"));
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::onReturnFromEditor()
{
    reload();
    showList();
}

//--------------------------------------------------------------------------------------------------
void ProfilesWidget::showList()
{
    stack_->setCurrentIndex(0);
    emit sig_titleChanged(tr("Profiles"));
    emit sig_appBarActionsChanged();
}
