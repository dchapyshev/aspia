//
// PROJECT:         Aspia
// FILE:            address_book/computer_group_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "address_book/computer_group_dialog.h"

#include <QtWidgets/QMessageBox>

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerGroupDialog::ComputerGroupDialog(QWidget* parent,
                                         ComputerGroup* group,
                                         ComputerGroup* parent_group)
    : QDialog(parent),
      group_(group)
{
    ui.setupUi(this);

    connect(ui.button_box,
            SIGNAL(clicked(QAbstractButton*)),
            this,
            SLOT(OnButtonBoxClicked(QAbstractButton*)));

    ui.edit_parent_name->setText(parent_group->Name());
    ui.edit_name->setText(group->Name());
    ui.edit_comment->setPlainText(group->Comment());
}

void ComputerGroupDialog::OnButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        QString name = ui.edit_name->text();
        if (name.length() > kMaxNameLength)
        {
            ShowError(tr("Too long name. The maximum length of the name is 64 characters."));
            return;
        }
        else if (name.length() < kMinNameLength)
        {
            ShowError(tr("Name can not be empty."));
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            ShowError(tr("Too long comment. The maximum length of the comment is 2048 characters."));
            return;
        }

        group_->SetName(name);
        group_->SetComment(comment);

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ComputerGroupDialog::ShowError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
