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

#include "router/ui/connect_dialog.h"

#include "qt_base/application.h"

#include <QAbstractButton>

namespace router {

namespace {

const int kMaxMruSize = 15;

} // namespace

ConnectDialog::ConnectDialog()
{
    qt_base::Application* instance = qt_base::Application::instance();
    instance->setLocale(settings_.locale());

    ui.setupUi(this);

    for (const auto& locale : instance->localeList())
        ui.combobox_language->addItem(locale.second, locale.first);

    int current_language = ui.combobox_language->findData(settings_.locale());
    if (current_language != -1)
        ui.combobox_language->setCurrentIndex(current_language);

    mru_ = settings_.mru();
    if (!mru_.isEmpty())
    {
        for (const auto& entry : mru_)
            ui.combobox_address->addItem(entry.address);

        ui.combobox_address->setCurrentIndex(0);

        const Settings::MruEntry& current_mru_entry = mru_.first();
        ui.spinbox_port->setValue(current_mru_entry.port);
        ui.edit_username->setText(current_mru_entry.username);
    }

    connect(ui.combobox_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectDialog::onCurrentLanguageChanged);

    connect(ui.combobox_address, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectDialog::onCurrentRouterChanged);

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &ConnectDialog::onButtonBoxClicked);

    ui.combobox_address->setFocus();
}

ConnectDialog::~ConnectDialog() = default;

QString ConnectDialog::address() const
{
    return ui.combobox_address->currentText();
}

uint16_t ConnectDialog::port() const
{
    return ui.spinbox_port->value();
}

QString ConnectDialog::userName() const
{
    return ui.edit_username->text();
}

QString ConnectDialog::password() const
{
    return ui.edit_password->text();
}

void ConnectDialog::closeEvent(QCloseEvent* /* event */)
{
    while (mru_.count() > kMaxMruSize)
        mru_.removeLast();

    settings_.setMru(mru_);
}

void ConnectDialog::onCurrentLanguageChanged(int index)
{
    QString locale = ui.combobox_language->itemData(index).toString();

    qt_base::Application::instance()->setLocale(locale);
    settings_.setLocale(locale);

    ui.retranslateUi(this);
}

void ConnectDialog::onCurrentRouterChanged(int index)
{
    if (index < 0 || index > mru_.count())
        return;

    Settings::MruEntry entry = mru_.at(index);

    ui.spinbox_port->setValue(entry.port);
    ui.edit_username->setText(entry.username);
}

void ConnectDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        QString current_address = address();

        for (int i = 0; i < mru_.count(); ++i)
        {
            if (current_address.compare(mru_[i].address, Qt::CaseInsensitive) == 0)
            {
                mru_.remove(i);
                break;
            }
        }

        Settings::MruEntry entry;

        entry.address = address();
        entry.port = port();
        entry.username = userName();

        mru_.insert(0, entry);

        reloadMru();
        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ConnectDialog::reloadMru()
{
    ui.combobox_address->clear();

    if (mru_.isEmpty())
        return;

    for (const auto& entry : mru_)
        ui.combobox_address->addItem(entry.address);

    ui.combobox_address->setCurrentIndex(0);
}

} // namespace router
