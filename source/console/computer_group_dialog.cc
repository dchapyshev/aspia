//
// PROJECT:         Aspia
// FILE:            console/computer_group_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_group_dialog.h"

#include <QAbstractButton>
#include <QMessageBox>

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

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerGroupDialog::buttonBoxClicked);

    ui.edit_parent_name->setText(parent_group->Name());
    ui.edit_name->setText(group->Name());
    ui.edit_comment->setPlainText(group->Comment());
}

ComputerGroupDialog::~ComputerGroupDialog() = default;

void ComputerGroupDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        QString name = ui.edit_name->text();
        if (name.length() > kMaxNameLength)
        {
            showError(tr("Too long name. The maximum length of the name is 64 characters."));
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
            showError(tr("Too long comment. The maximum length of the comment is 2048 characters."));
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

void ComputerGroupDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
