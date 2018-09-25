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

#include "console/address_book_dialog.h"

#include <QAbstractButton>
#include <QMessageBox>

#include "base/logging.h"
#include "crypto/password_hash.h"
#include "crypto/random.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMinPasswordLength = 1;
constexpr int kMaxPasswordLength = 64;
constexpr int kSafePasswordLength = 8;
constexpr int kMaxCommentLength = 2048;

bool isSafePassword(const QString& password)
{
    int length = password.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (int i = 0; i < length; ++i)
    {
        const QChar& character = password[i];

        has_upper |= character.isUpper();
        has_lower |= character.isLower();
        has_digit |= character.isDigit();
    }

    return has_upper && has_lower && has_digit;
}

} // namespace

AddressBookDialog::AddressBookDialog(QWidget* parent,
                                     const QString& file_path,
                                     proto::address_book::File* file,
                                     proto::address_book::Data* data,
                                     std::string* key)
    : QDialog(parent),
      file_(file),
      data_(data),
      key_(key)
{
    ui.setupUi(this);

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &AddressBookDialog::buttonBoxClicked);

    connect(ui.combo_encryption, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AddressBookDialog::encryptionTypedChanged);

    ui.combo_encryption->addItem(tr("Without Encryption"),
                                 QVariant(proto::address_book::ENCRYPTION_TYPE_NONE));
    ui.combo_encryption->addItem(tr("ChaCha20 + Poly1305 (256-bit key)"),
                                 QVariant(proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305));

    ui.edit_name->setText(QString::fromStdString(data_->root_group().name()));
    ui.edit_file->setText(file_path);
    ui.edit_comment->setPlainText(QString::fromStdString(data_->root_group().comment()));

    int current = ui.combo_encryption->findData(QVariant(file->encryption_type()));
    if (current != -1)
        ui.combo_encryption->setCurrentIndex(current);

    if (file->encryption_type() == proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305)
    {
        if (!key_->empty())
        {
            QString text = tr("Double-click to change");

            ui.edit_password->setText(text);
            ui.edit_password_repeat->setText(text);

            ui.edit_password->setEnabled(false);
            ui.edit_password_repeat->setEnabled(false);

            ui.edit_password->setEchoMode(QLineEdit::Normal);
            ui.edit_password->setInputMethodHints(Qt::ImhNone);

            ui.edit_password_repeat->setEchoMode(QLineEdit::Normal);
            ui.edit_password_repeat->setInputMethodHints(Qt::ImhNone);

            ui.edit_password->installEventFilter(this);
            ui.edit_password_repeat->installEventFilter(this);

            ui.spinbox_password_salt->setValue(file_->hashing_salt().size());

            ui.spinbox_salt_before->setValue(data_->salt1().size());
            ui.spinbox_salt_after->setValue(data_->salt2().size());

            password_changed_ = false;
        }
    }
    else
    {
        DCHECK_EQ(file->encryption_type(), proto::address_book::ENCRYPTION_TYPE_NONE);

        ui.edit_password->setEnabled(false);

        // Disable Advanced tab.
        ui.tab_widget->setTabEnabled(1, false);
    }

    connect(ui.spinbox_password_salt, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &AddressBookDialog::hashingSaltChanged);

    ui.edit_name->setFocus();
}

bool AddressBookDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui.edit_password || object == ui.edit_password_repeat))
    {
        proto::address_book::EncryptionType encryption_type =
            static_cast<proto::address_book::EncryptionType>(
                ui.combo_encryption->currentData().toInt());

        if (encryption_type == proto::address_book::ENCRYPTION_TYPE_NONE)
            return false;

        setPasswordChanged();
    }

    return false;
}

void AddressBookDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        close();
        return;
    }

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

    proto::address_book::EncryptionType encryption_type =
        static_cast<proto::address_book::EncryptionType>(
            ui.combo_encryption->currentData().toInt());

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
        {
            file_->mutable_hashing_salt()->clear();
            data_->mutable_salt1()->clear();
            data_->mutable_salt2()->clear();
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            if (password_changed_)
            {
                QString password = ui.edit_password->text();
                QString password_repeat = ui.edit_password_repeat->text();

                if (password != password_repeat)
                {
                    showError(tr("The passwords you entered do not match."));
                    return;
                }

                if (password.length() < kMinPasswordLength || password.length() > kMaxPasswordLength)
                {
                    showError(tr("Password can not be empty and should not exceed %n characters.",
                                 "", kMaxPasswordLength));
                    return;
                }

                if (!isSafePassword(password))
                {
                    if (QMessageBox::warning(this,
                                             tr("Warning"),
                                             tr("<b>Password you entered does not meet the security "
                                                "requirements!</b><br/>"
                                                "The password must contain lowercase and uppercase "
                                                "characters, numbers and should not be shorter "
                                                "than %n characters.<br/>"
                                                "Do you want to enter a different password?",
                                                "", kSafePasswordLength),
                                             QMessageBox::Yes,
                                             QMessageBox::No) == QMessageBox::Yes)
                    {
                        ui.edit_password->clear();
                        ui.edit_password_repeat->clear();
                        ui.edit_password->setFocus();
                        return;
                    }
                }

                // Generate salt, which is added after each iteration of the hashing.
                // New salt is generated each time the password is changed.
                file_->set_hashing_salt(Random::generateBuffer(ui.spinbox_password_salt->value()));

                // Now generate a key for encryption/decryption.
                *key_ = PasswordHash::hash(
                    PasswordHash::SCRYPT, password.toStdString(), file_->hashing_salt());
            }

            int salt_before_size = ui.spinbox_salt_before->value();
            int salt_after_size = ui.spinbox_salt_after->value();

            if (salt_before_size != data_->salt1().size())
                data_->set_salt1(Random::generateBuffer(salt_before_size));

            if (salt_after_size != data_->salt2().size())
                data_->set_salt2(Random::generateBuffer(salt_after_size));
        }
        break;

        default:
            LOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
            return;
    }

    data_->mutable_root_group()->set_name(name.toStdString());
    data_->mutable_root_group()->set_comment(comment.toStdString());

    file_->set_encryption_type(encryption_type);

    accept();
    close();
}

void AddressBookDialog::encryptionTypedChanged(int item_index)
{
    proto::address_book::EncryptionType encryption_type =
        static_cast<proto::address_book::EncryptionType>(
            ui.combo_encryption->itemData(item_index).toInt());

    switch (encryption_type)
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
        {
            ui.edit_password->setEnabled(false);
            ui.edit_password_repeat->setEnabled(false);

            ui.edit_password->clear();
            ui.edit_password_repeat->clear();

            // Disable Advanced tab.
            ui.tab_widget->setTabEnabled(1, false);
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            ui.edit_password->setEnabled(true);
            ui.edit_password_repeat->setEnabled(true);

            // Enable Advanced tab.
            ui.tab_widget->setTabEnabled(1, true);
        }
        break;

        default:
            LOG(LS_FATAL) << "Unexpected encryption type: " << encryption_type;
            break;
    }
}

void AddressBookDialog::hashingSaltChanged(int /* value */)
{
    if (password_changed_ || value_reverting_)
        return;

    if (QMessageBox::question(
        this,
        tr("Confirmation"),
        tr("At change the size of hashing salt, you will need to re-enter the password. Continue?"),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        setPasswordChanged();
    }
    else
    {
        // Revert value.
        value_reverting_ = true;
        ui.spinbox_password_salt->setValue(file_->hashing_salt().size());
        value_reverting_ = false;
    }
}

void AddressBookDialog::setPasswordChanged()
{
    password_changed_ = true;

    ui.edit_password->setEnabled(true);
    ui.edit_password_repeat->setEnabled(true);

    ui.edit_password->clear();
    ui.edit_password_repeat->clear();

    Qt::InputMethodHints hints = Qt::ImhHiddenText | Qt::ImhSensitiveData |
        Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;

    ui.edit_password->setEchoMode(QLineEdit::Password);
    ui.edit_password->setInputMethodHints(hints);

    ui.edit_password_repeat->setEchoMode(QLineEdit::Password);
    ui.edit_password_repeat->setInputMethodHints(hints);

    ui.edit_password->setFocus();
}

void AddressBookDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
