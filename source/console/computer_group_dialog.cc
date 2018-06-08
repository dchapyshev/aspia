//
// PROJECT:         Aspia
// FILE:            console/computer_group_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerGroupDialog::buttonBoxClicked);

    ui.edit_parent_name->setText(QString::fromStdString(parent_computer_group->name()));
    ui.edit_name->setText(QString::fromStdString(computer_group_->name()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer_group->comment()));
}

ComputerGroupDialog::~ComputerGroupDialog() = default;

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
