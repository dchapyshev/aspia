//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/router_dialog.h"

#include "qt_base/application.h"
#include "router/manager/main_window.h"

#include <QAbstractButton>
#include <QFileDialog>
#include <QMessageBox>

namespace router {

namespace {

const int kMaxMruSize = 15;

} // namespace

RouterDialog::RouterDialog()
    : mru_(kMaxMruSize)
{
    qt_base::Application* instance = qt_base::Application::instance();
    instance->setLocale(settings_.locale());

    ui.setupUi(this);

    for (const auto& locale : instance->localeList())
        ui.combobox_language->addItem(locale.second, locale.first);

    int current_language = ui.combobox_language->findData(settings_.locale());
    if (current_language != -1)
        ui.combobox_language->setCurrentIndex(current_language);

    mru_ = settings_.mru(kMaxMruSize);
    if (!mru_.isEmpty())
    {
        for (const auto& entry : mru_)
            ui.combobox_address->addItem(entry.address);

        ui.combobox_address->setCurrentIndex(0);

        auto current_mru_entry = mru_.cbegin();
        ui.spinbox_port->setValue(current_mru_entry->port);
        ui.edit_username->setText(current_mru_entry->username);
    }

    connect(ui.combobox_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouterDialog::onCurrentLanguageChanged);

    connect(ui.combobox_address, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouterDialog::onCurrentRouterChanged);

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &RouterDialog::onButtonBoxClicked);

    connect(ui.button_public_key, &QPushButton::clicked, [this]()
    {
        QString file_path =
            QFileDialog::getOpenFileName(this,
                                         tr("Open Public Key"),
                                         QString(),
                                         tr("Public Key (*.txt)"));
        if (file_path.isEmpty())
            return;

        ui.edit_public_key->setText(file_path);
    });

    ui.combobox_address->setFocus();
}

RouterDialog::~RouterDialog() = default;

void RouterDialog::closeEvent(QCloseEvent* /* event */)
{
    settings_.setMru(mru_);
}

void RouterDialog::onCurrentLanguageChanged(int index)
{
    QString locale = ui.combobox_language->itemData(index).toString();

    qt_base::Application::instance()->setLocale(locale);
    settings_.setLocale(locale);

    ui.retranslateUi(this);
}

void RouterDialog::onCurrentRouterChanged(int index)
{
    if (!mru_.hasIndex(index))
        return;

    const MruCache::Entry& entry = mru_.at(index);

    ui.spinbox_port->setValue(entry.port);
    ui.edit_username->setText(entry.username);
}

void RouterDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        QString address = ui.combobox_address->currentText();
        if (address.isEmpty())
        {
            showWarning(tr("You must enter a address."));
            ui.combobox_address->setFocus();
            return;
        }

        QString username = ui.edit_username->text();
        if (username.isEmpty())
        {
            showWarning(tr("You must enter a user name."));
            ui.edit_username->setFocus();
            return;
        }

        QString password = ui.edit_password->text();
        if (password.isEmpty())
        {
            showWarning(tr("You must enter a password."));
            ui.edit_password->setFocus();
            return;
        }

        QString file_path = ui.edit_public_key->text();
        if (file_path.isEmpty())
        {
            showWarning(tr("The path to the public key file must be specified."));
            ui.edit_public_key->setFocus();
            return;
        }

        int64_t file_size = QFileInfo(file_path).size();
        if (file_size <= 0 || file_size > 512)
        {
            showWarning(tr("The public key file has an invalid size."));
            ui.edit_public_key->setFocus();
            return;
        }

        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly))
        {
            showWarning(tr("Unable to open public key file \"%1\".").arg(file_path));
            ui.edit_public_key->setFocus();
            return;
        }

        QByteArray public_key = QByteArray::fromHex(file.readAll());
        MruCache::Entry entry;

        auto result = mru_.find(address);
        if (result != mru_.end())
        {
            entry = *result;
        }
        else
        {
            entry.address = address;
            entry.port = ui.spinbox_port->value();
            entry.username = username;
            entry.key_path = file_path;
        }

        main_window_ = new MainWindow(this);
        main_window_->connectToRouter(
            entry.address, entry.port, public_key, entry.username, password);

        mru_.put(std::move(entry));
        reloadMru();
        hide();
        return;
    }

    close();
}

void RouterDialog::reloadMru()
{
    ui.combobox_address->clear();

    if (mru_.isEmpty())
        return;

    for (const auto& entry : mru_)
        ui.combobox_address->addItem(entry.address);

    ui.combobox_address->setCurrentIndex(0);
}

void RouterDialog::showWarning(const QString& message)
{
    QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
}

} // namespace router
