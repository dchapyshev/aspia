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

        if (status_label_)
        {
            QString dots;

            switch (animation_index_)
            {
                case 0: dots = "   "; break;
                case 1: dots = ".  "; break;
                case 2: dots = ".. "; break;
                case 3: dots = "..."; break;
                default: break;
            }

            status_label_->setText(tr("Status update") + dots);
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

    QLabel* first_label = new QLabel(QString::fromStdString(computer_group.name()), this);
    QLabel* second_label = new QLabel(child_groups, this);
    QLabel* third_label = new QLabel(child_computers, this);

    status_label_ = new QLabel(QString(), this);

    static const QString kStyle = QStringLiteral("QLabel { padding: 3px; }");

    first_label->setStyleSheet(kStyle);
    second_label->setStyleSheet(kStyle);
    third_label->setStyleSheet(kStyle);
    status_label_->setStyleSheet(kStyle);

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
