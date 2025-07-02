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

#include "console/computer_dialog.h"

#include <QAbstractButton>
#include <QDateTime>

#include "base/logging.h"
#include "base/crypto/secure_memory.h"
#include "client/config_factory.h"
#include "console/computer_dialog_desktop.h"
#include "console/computer_dialog_general.h"
#include "console/computer_dialog_parent.h"
#include "console/computer_dialog_port_forwarding.h"
#include "console/computer_factory.h"

namespace console {

namespace {

enum ItemType
{
    ITEM_TYPE_PARENT,
    ITEM_TYPE_GENERAL,
    ITEM_TYPE_DESKTOP_MANAGE,
    ITEM_TYPE_DESKTOP_VIEW,
    ITEM_TYPE_PORT_FORWARDING
};

} // namespace

//--------------------------------------------------------------------------------------------------
ComputerDialog::ComputerDialog(QWidget* parent,
                               Mode mode,
                               const QString& parent_name,
                               const std::optional<proto::address_book::Computer>& computer)
    : QDialog(parent),
      mode_(mode),
      computer_(computer.value_or(ComputerFactory::defaultComputer()))
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    client::ConfigFactory::fixupDesktopConfig(
        computer_.mutable_session_config()->mutable_desktop_manage());
    client::ConfigFactory::fixupDesktopConfig(
        computer_.mutable_session_config()->mutable_desktop_view());

    if (mode_ == Mode::COPY)
    {
        computer_.set_name(computer_.name() + ' ' + tr("(copy)").toStdString());
    }

    restoreGeometry(settings_.computerDialogGeometry());
    ui.splitter->restoreState(settings_.computerDialogState());

    connect(ui.tree, &QTreeWidget::currentItemChanged, this, &ComputerDialog::onTabChanged);
    connect(ui.button_box, &QDialogButtonBox::clicked, this, &ComputerDialog::buttonBoxClicked);

    QTreeWidgetItem* general_item = new QTreeWidgetItem(ITEM_TYPE_GENERAL);
    general_item->setIcon(0, QIcon(":/img/computer.svg"));
    general_item->setText(0, tr("General"));

    QTreeWidgetItem* sessions_item = new QTreeWidgetItem(ITEM_TYPE_PARENT);
    sessions_item->setIcon(0, QIcon(":/img/settings.svg"));
    sessions_item->setText(0, tr("Sessions"));

    ui.tree->addTopLevelItem(general_item);
    ui.tree->addTopLevelItem(sessions_item);

    QTreeWidgetItem* desktop_manage_item = new QTreeWidgetItem(ITEM_TYPE_DESKTOP_MANAGE);
    desktop_manage_item->setIcon(0, QIcon(":/img/workstation.svg"));
    desktop_manage_item->setText(0, tr("Desktop Manage"));

    QTreeWidgetItem* desktop_view_item = new QTreeWidgetItem(ITEM_TYPE_DESKTOP_VIEW);
    desktop_view_item->setIcon(0, QIcon(":/img/computer.svg"));
    desktop_view_item->setText(0, tr("Desktop View"));

    QTreeWidgetItem* port_forwarding_item = new QTreeWidgetItem(ITEM_TYPE_PORT_FORWARDING);
    port_forwarding_item->setIcon(0, QIcon(":/img/ethernet-off.svg"));
    port_forwarding_item->setText(0, tr("Port Forwarding"));

    sessions_item->addChild(desktop_manage_item);
    sessions_item->addChild(desktop_view_item);
    sessions_item->addChild(port_forwarding_item);

    ComputerDialogParent* parent_tab =
        new ComputerDialogParent(ITEM_TYPE_PARENT, ui.widget);
    ComputerDialogGeneral* general_tab =
        new ComputerDialogGeneral(ITEM_TYPE_GENERAL, ui.widget);
    ComputerDialogDesktop* desktop_manage_tab =
        new ComputerDialogDesktop(ITEM_TYPE_DESKTOP_MANAGE, ui.widget);
    ComputerDialogDesktop* desktop_view_tab =
        new ComputerDialogDesktop(ITEM_TYPE_DESKTOP_VIEW, ui.widget);
    ComputerDialogPortForwarding* port_forwarding_tab =
        new ComputerDialogPortForwarding(ITEM_TYPE_PORT_FORWARDING, ui.widget);

    general_tab->restoreSettings(parent_name, computer_);
    desktop_manage_tab->restoreSettings(proto::peer::SESSION_TYPE_DESKTOP_MANAGE, computer_);
    desktop_view_tab->restoreSettings(proto::peer::SESSION_TYPE_DESKTOP_VIEW, computer_);
    port_forwarding_tab->restoreSettings(computer_);

    tabs_.append(general_tab);
    tabs_.append(desktop_manage_tab);
    tabs_.append(desktop_view_tab);
    tabs_.append(port_forwarding_tab);
    tabs_.append(parent_tab);

    QSize min_size;

    for (auto it = tabs_.begin(), it_end = tabs_.end(); it != it_end; ++it)
    {
        QWidget* tab = *it;
        min_size.setWidth(std::max(tab->sizeHint().width(), min_size.width()));
        min_size.setHeight(std::max(tab->minimumSizeHint().height(), min_size.height()));
    }

    ui.widget->setMinimumSize(min_size);
    ui.widget->installEventFilter(this);

    ui.tree->setCurrentItem(general_item);
    ui.tree->expandAll();
}

//--------------------------------------------------------------------------------------------------
ComputerDialog::~ComputerDialog()
{
    LOG(INFO) << "Dtor";
    base::memZero(computer_.mutable_name());
    base::memZero(computer_.mutable_address());
    base::memZero(computer_.mutable_username());
    base::memZero(computer_.mutable_password());
    base::memZero(computer_.mutable_comment());
}

//--------------------------------------------------------------------------------------------------
void ComputerDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event";
    settings_.setComputerDialogGeometry(saveGeometry());
    settings_.setComputerDialogState(ui.splitter->saveState());
    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
bool ComputerDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui.widget && event->type() == QEvent::Resize)
    {
        for (auto it = tabs_.begin(), it_end = tabs_.end(); it != it_end; ++it)
        {
            QWidget* tab = *it;
            tab->resize(ui.widget->size());
        }
    }

    return QDialog::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialog::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Return) && (event->modifiers() & Qt::ControlModifier))
    {
        if (saveChanges())
        {
            accept();
            close();
        }
    }

    QDialog::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void ComputerDialog::onTabChanged(QTreeWidgetItem* current)
{
    if (current)
        showTab(current->type());
}

//--------------------------------------------------------------------------------------------------
void ComputerDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (!saveChanges())
            return;

        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void ComputerDialog::showTab(int type)
{
    for (auto it = tabs_.begin(), it_end = tabs_.end(); it != it_end; ++it)
    {
        QWidget* tab = *it;
        if (static_cast<ComputerDialogTab*>(tab)->type() == type)
            tab->show();
        else
            tab->hide();
    }
}

//--------------------------------------------------------------------------------------------------
bool ComputerDialog::saveChanges()
{
    for (auto it = tabs_.begin(), it_end = tabs_.end(); it != it_end; ++it)
    {
        QWidget* tab = *it;
        int type = static_cast<ComputerDialogTab*>(tab)->type();

        if (type == ITEM_TYPE_GENERAL)
        {
            ComputerDialogGeneral* general_tab = static_cast<ComputerDialogGeneral*>(tab);
            if (!general_tab->saveSettings(&computer_))
                return false;
        }
        else if (type == ITEM_TYPE_DESKTOP_MANAGE)
        {
            ComputerDialogDesktop* desktop_tab = static_cast<ComputerDialogDesktop*>(tab);
            desktop_tab->saveSettings(proto::peer::SESSION_TYPE_DESKTOP_MANAGE, &computer_);
        }
        else if (type == ITEM_TYPE_DESKTOP_VIEW)
        {
            ComputerDialogDesktop* desktop_tab = static_cast<ComputerDialogDesktop*>(tab);
            desktop_tab->saveSettings(proto::peer::SESSION_TYPE_DESKTOP_VIEW, &computer_);
        }
        else if (type == ITEM_TYPE_PORT_FORWARDING)
        {
            ComputerDialogPortForwarding* port_forwarding_tab =
                static_cast<ComputerDialogPortForwarding*>(tab);
            port_forwarding_tab->saveSettings(&computer_);
        }
    }

    qint64 current_time = QDateTime::currentSecsSinceEpoch();

    if (mode_ == Mode::CREATE || mode_ == Mode::COPY)
        computer_.set_create_time(current_time);

    computer_.set_modify_time(current_time);
    return true;
}

} // namespace console
