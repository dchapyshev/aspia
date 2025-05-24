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

#include "console/statusbar.h"

#include <QIcon>

namespace console {

//--------------------------------------------------------------------------------------------------
StatusBar::StatusBar(QWidget* parent)
    : QStatusBar(parent),
      animation_timer_(new QTimer(this))
{
    connect(animation_timer_, &QTimer::timeout, this, [this]()
    {
        if (animation_index_ > 3)
            animation_index_ = 0;

        QString icon;

        switch (animation_index_)
        {
            case 0:
                icon = QStringLiteral(":/img/arrow-circle.png");
                break;

            case 1:
                icon = QStringLiteral(":/img/arrow-circle-315.png");
                break;

            case 2:
                icon = QStringLiteral(":/img/arrow-circle-225.png");
                break;

            case 3:
                icon = QStringLiteral(":/img/arrow-circle-135.png");
                break;

            default:
                return;
        }

        if (status_label_)
        {
            status_label_->setText(
                QStringLiteral("<table><tr><td><img src='%1'></td><td>%2</td></tr></table>")
                .arg(icon, tr("Status update...")));
        }

        ++animation_index_;
    });
}

//--------------------------------------------------------------------------------------------------
void StatusBar::setCurrentComputerGroup(
    const proto::address_book::ComputerGroup& computer_group)
{
    clear();

    QString child_groups = tr("%n child group(s)", "", computer_group.computer_group_size());
    QString child_computers = tr("%n child computer(s)", "", computer_group.computer_size());

    QLabel* first_label = new QLabel(
        QString("<table><tr><td><img src=':/img/folder.png'></td><td>%1</td></tr></table>")
        .arg(QString::fromStdString(computer_group.name())), this);

    QLabel* second_label = new QLabel(
        QString("<table><tr><td><img src=':/img/folder.png'></td><td>%1</td></tr></table>")
        .arg(child_groups), this);

    QLabel* third_label = new QLabel(
        QString("<table><tr><td><img src=':/img/computer.png'></td><td>%1</td></tr></table>")
        .arg(child_computers), this);

    status_label_ = new QLabel(QString(), this);

    first_label->setTextFormat(Qt::RichText);
    second_label->setTextFormat(Qt::RichText);
    third_label->setTextFormat(Qt::RichText);
    status_label_->setTextFormat(Qt::RichText);

    addWidget(first_label);
    addWidget(second_label);
    addWidget(third_label);
    addWidget(status_label_);

    status_label_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void StatusBar::clear()
{
    animation_timer_->stop();
    animation_index_ = 0;

    QList<QLabel*> items = findChildren<QLabel*>();

    for (const auto& item : std::as_const(items))
    {
        removeWidget(item);
        delete item;
    }
}

//--------------------------------------------------------------------------------------------------
void StatusBar::setUpdateState(bool enable)
{
    if (status_label_)
        status_label_->setVisible(enable);

    if (enable)
    {
        animation_timer_->start(std::chrono::milliseconds(200));
    }
    else
    {
        if (status_label_)
            status_label_->setText(QString());
        animation_timer_->stop();
        animation_index_ = 0;
    }
}

} // namespace console
