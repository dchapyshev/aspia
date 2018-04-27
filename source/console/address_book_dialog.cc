//
// PROJECT:         Aspia
// FILE:            console/address_book_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/address_book_dialog.h"

#include <QMessageBox>

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 8;
constexpr int kMaxPasswordLength = 64;
constexpr int kMaxCommentLength = 2048;

} // namespace

AddressBookDialog::AddressBookDialog(QWidget* parent,
                                     proto::AddressBook::EncryptionType* encryption_type,
                                     QString* password,
                                     proto::ComputerGroup* root_group)
    : QDialog(parent),
      root_group_(root_group),
      encryption_type_(encryption_type),
      password_(password)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &AddressBookDialog::buttonBoxClicked);

    connect(ui.combo_encryption, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddressBookDialog::encryptionTypedChanged);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AddressBookDialog::showPasswordButtonToggled);

    ui.combo_encryption->addItem(tr("Without Encryption"),
                                 QVariant(proto::AddressBook::ENCRYPTION_TYPE_NONE));
    ui.combo_encryption->addItem(tr("XChaCha20 + Poly1305 (256-bit key)"),
                                 QVariant(proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305));

    ui.edit_name->setText(QString::fromStdString(root_group_->name()));
    ui.edit_comment->setPlainText(QString::fromStdString(root_group_->comment()));

    int current = ui.combo_encryption->findData(QVariant(*encryption_type_));
    if (current != -1)
        ui.combo_encryption->setCurrentIndex(current);

    if (*encryption_type_ == proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305)
    {
        ui.edit_password->setText(*password);
    }
    else
    {
        Q_ASSERT(*encryption_type_ == proto::AddressBook::ENCRYPTION_TYPE_NONE);
        ui.edit_password->setEnabled(false);
    }
}

AddressBookDialog::~AddressBookDialog() = default;

void AddressBookDialog::buttonBoxClicked(QAbstractButton* button)
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

        proto::AddressBook::EncryptionType encryption_type =
            static_cast<proto::AddressBook::EncryptionType>(
                ui.combo_encryption->currentData().toInt());
        QString password;

        switch (encryption_type)
        {
            case proto::AddressBook::ENCRYPTION_TYPE_NONE:
                break;

            case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
            {
                password = ui.edit_password->text();
                if (password.length() < kMinPasswordLength)
                {
                    showError(tr("Password can not be shorter than 8 characters."));
                    return;
                }
            }
            break;

            default:
                qFatal("Unexpected encryption type: %d", encryption_type);
                break;
        }

        root_group_->set_name(name.toStdString());
        root_group_->set_comment(comment.toStdString());

        *encryption_type_ = encryption_type;
        *password_ = password;

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void AddressBookDialog::encryptionTypedChanged(int item_index)
{
    proto::AddressBook::EncryptionType encryption_type =
        static_cast<proto::AddressBook::EncryptionType>(
            ui.combo_encryption->itemData(item_index).toInt());

    switch (encryption_type)
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
            ui.edit_password->setEnabled(false);
            ui.button_show_password->setEnabled(false);
            break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
            ui.edit_password->setEnabled(true);
            ui.button_show_password->setEnabled(true);
            break;

        default:
            qFatal("Unexpected encryption type: %d", encryption_type);
            break;
    }
}

void AddressBookDialog::showPasswordButtonToggled(bool checked)
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

void AddressBookDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
