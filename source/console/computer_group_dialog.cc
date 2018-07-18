//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/computer_group_dialog.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QMessageBox>

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerGroupDialog::ComputerGroupDialog(QWidget* parent,
                                         Mode mode,
                                         proto::address_book::ComputerGroup* computer_group,
                                         proto::address_book::ComputerGroup* parent_computer_group)
    : QDialog(parent),
      mode_(mode),
      computer_group_(computer_group)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerGroupDialog::buttonBoxClicked);

    ui.edit_parent_name->setText(QString::fromStdString(parent_computer_group->name()));
    ui.edit_name->setText(QString::fromStdString(computer_group_->name()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer_group->comment()));
}

void ComputerGroupDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        QString name = ui.edit_name->text();
        if (name.length() > kMaxNameLength)
        {
            showError(tr("Too long name. The maximum length of the name is %n characters.",
                         "", kMaxNameLength));
            return;
        }
        else if (name.length() < kMinNameLength)
        {
            showError(tr("Name can not be empty."));
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                         "", kMaxCommentLength));
            return;
        }

        qint64 current_time = QDateTime::currentSecsSinceEpoch();

        if (mode_ == CreateComputerGroup)
            computer_group_->set_create_time(current_time);

        computer_group_->set_modify_time(current_time);
        computer_group_->set_name(name.toStdString());
        computer_group_->set_comment(comment.toStdString());

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ComputerGroupDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
