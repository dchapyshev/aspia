//
// PROJECT:         Aspia
// FILE:            console/computer_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer_dialog.h"

#include <QMessageBox>

#include "client/ui/desktop_config_dialog.h"
#include "console/computer_group.h"
#include "host/user.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialog::ComputerDialog(QWidget* parent, Computer* computer, ComputerGroup* parent_group)
    : QDialog(parent),
      computer_(computer)
{
    ui.setupUi(this);

    ui.combo_session_config->addItem(QIcon(":/icon/monitor-keyboard.png"),
                                     tr("Desktop Manage"),
                                     QVariant(proto::auth::SESSION_TYPE_DESKTOP_MANAGE));

    ui.combo_session_config->addItem(QIcon(":/icon/monitor.png"),
                                     tr("Desktop View"),
                                     QVariant(proto::auth::SESSION_TYPE_DESKTOP_VIEW));

    ui.combo_session_config->addItem(QIcon(":/icon/folder-stand.png"),
                                     tr("File Transfer"),
                                     QVariant(proto::auth::SESSION_TYPE_FILE_TRANSFER));

#if 0
    ui.combo_session_config->addItem(QIcon(":/icon/system-monitor.png"),
                                     tr("System Information"),
                                     QVariant(proto::auth::SESSION_TYPE_SYSTEM_INFO));
#endif

    ui.edit_parent_name->setText(parent_group->Name());
    ui.edit_name->setText(computer->Name());
    ui.edit_address->setText(computer->Address());
    ui.spinbox_port->setValue(computer->Port());
    ui.edit_username->setText(computer->UserName());
    ui.edit_password->setText(computer->Password());
    ui.edit_comment->setPlainText(computer->Comment());

    connect(ui.combo_session_config, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerDialog::OnSessionTypeChanged);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerDialog::OnShowPasswordButtonToggled);

    connect(ui.button_session_config, &QPushButton::pressed,
            this, &ComputerDialog::OnSessionConfigButtonPressed);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerDialog::OnButtonBoxClicked);
}

void ComputerDialog::OnSessionTypeChanged(int item_index)
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

void ComputerDialog::OnShowPasswordButtonToggled(bool checked)
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

void ComputerDialog::OnSessionConfigButtonPressed()
{
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(ui.combo_session_config->currentData().toInt());

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        {
            proto::desktop::Config config = computer_->DesktopManageSessionConfig();

            DesktopConfigDialog dialog(session_type, &config, this);
            if (dialog.exec() == QDialog::Accepted)
                computer_->SetDesktopManageSessionConfig(config);
        }
        break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
        {
            proto::desktop::Config config = computer_->DesktopViewSessionConfig();

            DesktopConfigDialog dialog(session_type, &config, this);
            if (dialog.exec() == QDialog::Accepted)
                computer_->SetDesktopViewSessionConfig(config);
        }
        break;

        default:
            break;
    }
}

void ComputerDialog::OnButtonBoxClicked(QAbstractButton* button)
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

        QString username = ui.edit_username->text();
        if (!username.isEmpty() && !User::isValidName(username))
        {
            ShowError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            return;
        }

        QString password = ui.edit_password->text();
        if (!password.isEmpty() && !User::isValidPassword(password))
        {
            ShowError(tr("Password can not be shorter than 8 characters."));
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            ShowError(tr("Too long comment. The maximum length of the comment is 2048 characters."));
            return;
        }

        computer_->SetName(name);
        computer_->SetAddress(ui.edit_address->text());
        computer_->SetPort(ui.spinbox_port->value());
        computer_->SetUserName(username);
        computer_->SetPassword(password);
        computer_->SetComment(comment);

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ComputerDialog::ShowError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
