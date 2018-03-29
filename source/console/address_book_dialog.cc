//
// PROJECT:         Aspia
// FILE:            console/address_book_dialog.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/address_book_dialog.h"

#include <QMessageBox>

#include "base/logging.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 8;
constexpr int kMaxPasswordLength = 64;
constexpr int kMaxCommentLength = 2048;

} // namespace

AddressBookDialog::AddressBookDialog(QWidget* parent,
                                     AddressBook* address_book,
                                     proto::AddressBook::EncryptionType* encryption_type,
                                     QString* password)
    : QDialog(parent),
      address_book_(address_book),
      encryption_type_(encryption_type),
      password_(password)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &AddressBookDialog::OnButtonBoxClicked);

    connect(ui.combo_encryption, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddressBookDialog::OnEncryptionTypedChanged);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AddressBookDialog::OnShowPasswordButtonToggled);

    ui.combo_encryption->addItem(tr("Without Encryption"),
                                 QVariant(proto::AddressBook::ENCRYPTION_TYPE_NONE));
    ui.combo_encryption->addItem(tr("XChaCha20 + Poly1305 (256-bit key)"),
                                 QVariant(proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305));

    ui.edit_name->setText(address_book_->Name());
    ui.edit_comment->setPlainText(address_book_->Comment());

    int current = ui.combo_encryption->findData(QVariant(*encryption_type_));
    if (current != -1)
        ui.combo_encryption->setCurrentIndex(current);

    if (*encryption_type_ == proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305)
    {
        ui.edit_password->setText(*password);
    }
    else
    {
        DCHECK_EQ(*encryption_type_, proto::AddressBook::ENCRYPTION_TYPE_NONE);
        ui.edit_password->setEnabled(false);
    }
}

void AddressBookDialog::OnButtonBoxClicked(QAbstractButton* button)
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
                    ShowError(tr("Password can not be shorter than 8 characters."));
                    return;
                }
            }
            break;

            default:
                DLOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
                break;
        }

        address_book_->SetName(name);
        address_book_->SetComment(comment);
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

void AddressBookDialog::OnEncryptionTypedChanged(int item_index)
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
            DLOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
            break;
    }
}

void AddressBookDialog::OnShowPasswordButtonToggled(bool checked)
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

void AddressBookDialog::ShowError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
