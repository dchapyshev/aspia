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

#include "host/ui/permission_dialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const int kRefreshIntervalMs = 1000;

} // namespace

//--------------------------------------------------------------------------------------------------
PermissionDialog::PermissionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Review System Access"));

    QVBoxLayout* main_layout = new QVBoxLayout(this);

    QLabel* title = new QLabel(
        tr("Aspia Host requires your permission to access system capabilities."), this);
    QFont title_font = title->font();
    title_font.setBold(true);
    title->setFont(title_font);

    QLabel* subtitle = new QLabel(
        tr("Review the permissions below and grant the ones that are missing."), this);
    subtitle->setWordWrap(true);

    main_layout->addWidget(title);
    main_layout->addWidget(subtitle);
    main_layout->addSpacing(8);

    grid_ = new QGridLayout();
    grid_->setColumnStretch(0, 1);
    grid_->setHorizontalSpacing(16);
    grid_->setVerticalSpacing(12);
    main_layout->addLayout(grid_);

    addPermission(MacPermission::SCREEN_RECORDING, tr("Screen Recording"),
        tr("Required so a remote user can see this screen."), tr("Request Access..."));
    addPermission(MacPermission::ACCESSIBILITY, tr("Accessibility"),
        tr("Required so a remote user can control the mouse and keyboard."), tr("Request Access..."));
    addPermission(MacPermission::FULL_DISK_ACCESS, tr("Full Disk Access"),
        tr("Required to transfer files from protected folders."), tr("Open Preferences..."));

    main_layout->addStretch();

    QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main_layout->addWidget(button_box);

    QTimer* refresh_timer = new QTimer(this);
    connect(refresh_timer, &QTimer::timeout, this, &PermissionDialog::refresh);
    refresh_timer->start(kRefreshIntervalMs);

    refresh();
}

//--------------------------------------------------------------------------------------------------
PermissionDialog::~PermissionDialog() = default;

//--------------------------------------------------------------------------------------------------
// static
bool PermissionDialog::hasMissingPermissions()
{
    return !hasMacPermission(MacPermission::SCREEN_RECORDING) ||
           !hasMacPermission(MacPermission::ACCESSIBILITY) ||
           !hasMacPermission(MacPermission::FULL_DISK_ACCESS);
}

//--------------------------------------------------------------------------------------------------
void PermissionDialog::addPermission(MacPermission permission, const QString& title,
                                     const QString& description, const QString& action)
{
    const int index = static_cast<int>(items_.size());

    QLabel* name = new QLabel(QString("<b>%1</b><br>%2").arg(title, description), this);
    name->setTextFormat(Qt::RichText);

    QLabel* status = new QLabel(this);

    QPushButton* button = new QPushButton(action, this);
    connect(button, &QPushButton::clicked, this, [this, index]()
    {
        Item& item = items_[index];

        // The Screen Recording / Accessibility system prompt appears only once per launch, so the first
        // press requests it and later presses open the Privacy pane where the user can toggle the app.
        if (item.requested)
        {
            openMacPermissionSettings(item.permission);
        }
        else
        {
            requestMacPermission(item.permission);
            item.requested = true;
            item.button->setText(tr("Open Settings..."));
        }

        refresh();
    });

    grid_->addWidget(name, index, 0);
    grid_->addWidget(status, index, 1, Qt::AlignCenter);
    grid_->addWidget(button, index, 2);

    items_.append({ permission, status, button, false });
}

//--------------------------------------------------------------------------------------------------
void PermissionDialog::refresh()
{
    for (const Item& item : std::as_const(items_))
    {
        const bool granted = hasMacPermission(item.permission);

        item.status->setText(granted ? tr("Granted") : tr("Denied"));
        item.status->setStyleSheet(granted ? "color: #2e7d32;" : "color: #c62828;");
        item.button->setVisible(!granted);
    }
}
