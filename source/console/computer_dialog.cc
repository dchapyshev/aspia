//
// PROJECT:         Aspia
// FILE:            console/computer_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_dialog.h"

#include <QMessageBox>

#include "client/ui/desktop_config_dialog.h"
#include "console/computer_group_item.h"
#include "host/user.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialog::ComputerDialog(QWidget* parent,
                               proto::Computer* computer,
                               proto::ComputerGroup* parent_computer_group)
    : QDialog(parent),
      computer_(computer)
{
    ui.setupUi(this);

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/icon/monitor-keyboard.png")),
                                     tr("Desktop Manage"),
                                     QVariant(proto::auth::SESSION_TYPE_DESKTOP_MANAGE));

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/icon/monitor.png")),
                                     tr("Desktop View"),
                                     QVariant(proto::auth::SESSION_TYPE_DESKTOP_VIEW));

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/icon/folder-stand.png")),
                                     tr("File Transfer"),
                                     QVariant(proto::auth::SESSION_TYPE_FILE_TRANSFER));

    ui.edit_parent_name->setText(QString::fromStdString(parent_computer_group->name()));
    ui.edit_name->setText(QString::fromStdString(computer_->name()));
    ui.edit_address->setText(QString::fromStdString(computer_->address()));
    ui.spinbox_port->setValue(computer_->port());
    ui.edit_username->setText(QString::fromStdString(computer_->username()));
    ui.edit_password->setText(QString::fromStdString(computer->password()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer->comment()));

    connect(ui.combo_session_config, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerDialog::sessionTypeChanged);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerDialog::showPasswordButtonToggled);

    connect(ui.button_session_config, &QPushButton::pressed,
            this, &ComputerDialog::sessionConfigButtonPressed);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerDialog::buttonBoxClicked);
}

ComputerDialog::~ComputerDialog() = default;

void ComputerDialog::sessionTypeChanged(int item_index)
{
    proto::auth::SessionType session_type = static_cast<proto::auth::SessionType>(
        ui.combo_session_config->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            ui.button_session_config->setEnabled(true);
            break;

        default:
            ui.button_session_config->setEnabled(false);
            break;
    }
}

void ComputerDialog::showPasswordButtonToggled(bool checked)
{
    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

void ComputerDialog::sessionConfigButtonPressed()
{
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(ui.combo_session_config->currentData().toInt());

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        {
            proto::desktop::Config config = computer_->desktop_manage_session();

            DesktopConfigDialog dialog(session_type, &config, this);
            if (dialog.exec() == QDialog::Accepted)
                computer_->mutable_desktop_manage_session()->CopyFrom(config);
        }
        break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        {
            proto::desktop::Config config = computer_->desktop_view_session();

            DesktopConfigDialog dialog(session_type, &config, this);
            if (dialog.exec() == QDialog::Accepted)
                computer_->mutable_desktop_view_session()->CopyFrom(config);
        }
        break;

        default:
            break;
    }
}

void ComputerDialog::buttonBoxClicked(QAbstractButton* button)
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

        QString username = ui.edit_username->text();
        if (!username.isEmpty() && !User::isValidName(username))
        {
            showError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            return;
        }

        QString password = ui.edit_password->text();
        if (!password.isEmpty() && !User::isValidPassword(password))
        {
            showError(tr("Password can not be shorter than 8 characters."));
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            showError(tr("Too long comment. The maximum length of the comment is 2048 characters."));
            return;
        }

        computer_->set_name(name.toStdString());
        computer_->set_address(ui.edit_address->text().toStdString());
        computer_->set_port(ui.spinbox_port->value());
        computer_->set_username(username.toStdString());
        computer_->set_password(password.toStdString());
        computer_->set_comment(comment.toStdString());

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ComputerDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
