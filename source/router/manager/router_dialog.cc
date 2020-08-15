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
#include <QMessageBox>

namespace router {

namespace {

const int kMaxMruSize = 15;

} // namespace

RouterDialog::RouterDialog()
    : mru_cache_(kMaxMruSize)
{
    qt_base::Application* instance = qt_base::Application::instance();
    instance->setLocale(settings_.locale());

    ui.setupUi(this);
    setFixedHeight(sizeHint().height());

    for (const auto& locale : instance->localeList())
        ui.combobox_language->addItem(locale.second, locale.first);

    int current_language = ui.combobox_language->findData(settings_.locale());
    if (current_language != -1)
        ui.combobox_language->setCurrentIndex(current_language);

    connect(ui.combobox_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouterDialog::onCurrentLanguageChanged);

    connect(ui.combobox_address, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RouterDialog::onCurrentRouterChanged);

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &RouterDialog::onButtonBoxClicked);

    settings_.readMru(&mru_cache_);
    reloadMru();

    ui.combobox_address->setFocus();
}

RouterDialog::~RouterDialog()
{
    settings_.writeMru(mru_cache_);
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
    if (!mru_cache_.hasIndex(index))
        return;

    const MruCache::Entry& entry = mru_cache_.at(index);

    ui.spinbox_port->setValue(entry.port);
    ui.edit_username->setText(entry.username);
}

void RouterDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        close();
        return;
    }

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

    uint16_t port = static_cast<uint16_t>(ui.spinbox_port->value());

    MruCache::Entry entry;
    entry.address = address;
    entry.port = port;
    entry.username = username;

    mru_cache_.put(std::move(entry));
    reloadMru();

    main_window_ = new MainWindow();
    main_window_->connectToRouter(address, port, username, password);

    hide();
}

void RouterDialog::reloadMru()
{
    ui.combobox_address->clear();

    if (mru_cache_.isEmpty())
        return;

    for (const auto& entry : mru_cache_)
        ui.combobox_address->addItem(entry.address);

    ui.combobox_address->setCurrentIndex(0);
}

void RouterDialog::showWarning(const QString& message)
{
    QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
}

} // namespace router
