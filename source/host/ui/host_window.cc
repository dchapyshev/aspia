//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/host_window.h"

#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QUrl>

#include "base/xml_settings.h"
#include "common/ui/about_dialog.h"
#include "common/ui/language_action.h"
#include "host/ui/host_config_dialog.h"
#include "host/host_settings.h"
#include "net/network_adapter_enumerator.h"

namespace host {

HostWindow::HostWindow(Settings& settings, common::LocaleLoader& locale_loader, QWidget* parent)
    : QMainWindow(parent),
      settings_(settings),
      locale_loader_(locale_loader)
{
    ui.setupUi(this);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);

    tray_menu_.addAction(ui.action_show_hide);
    tray_menu_.addSeparator();
    tray_menu_.addAction(ui.action_exit);

    tray_icon_.setIcon(QIcon(QStringLiteral(":/img/main.png")));
    tray_icon_.setToolTip(tr("Aspia Host"));
    tray_icon_.setContextMenu(&tray_menu_);
    tray_icon_.show();

    createLanguageMenu(settings.locale());
    refreshIpList();

    connect(ui.menu_language, &QMenu::triggered, this, &HostWindow::onLanguageChanged);
    connect(ui.action_settings, &QAction::triggered, this, &HostWindow::onSettings);
    connect(ui.action_show_hide, &QAction::triggered, this, &HostWindow::onShowHide);
    connect(ui.action_exit, &QAction::triggered, this, &HostWindow::close);
    connect(ui.action_help, &QAction::triggered, this, &HostWindow::onHelp);
    connect(ui.action_about, &QAction::triggered, this, &HostWindow::onAbout);

    connect(ui.button_refresh_ip_list, &QPushButton::released, this, &HostWindow::refreshIpList);
    connect(ui.button_new_password, &QPushButton::released, this, &HostWindow::newPassword);

    setFixedHeight(sizeHint().height());
}

HostWindow::~HostWindow() = default;

void HostWindow::refreshIpList()
{
    QString ip_list;

    for (net::AdapterEnumerator adapters; !adapters.isAtEnd(); adapters.advance())
    {
        for (net::AdapterEnumerator::IpAddressEnumerator addresses(adapters);
             !addresses.isAtEnd(); addresses.advance())
        {
            if (!ip_list.isEmpty())
                ip_list += QStringLiteral(", ");

            ip_list += QString::fromStdString(addresses.address());
        }
    }

    ui.edit_ip->setText(ip_list);
}

void HostWindow::newPassword()
{
    // TODO
}

void HostWindow::onLanguageChanged(QAction* action)
{
    common::LanguageAction* language_action = dynamic_cast<common::LanguageAction*>(action);
    if (!language_action)
        return;

    QString new_locale = language_action->locale();
    locale_loader_.installTranslators(new_locale);

    ui.retranslateUi(this);
    settings_.setLocale(new_locale);
}

void HostWindow::onSettings()
{
    HostConfigDialog(this).exec();
}

void HostWindow::onShowHide()
{
    if (isVisible())
    {
        ui.action_show_hide->setText(tr("Show"));
        setVisible(false);
    }
    else
    {
        ui.action_show_hide->setText(tr("Hide"));
        setVisible(true);
    }
}

void HostWindow::onHelp()
{
    QDesktopServices::openUrl(QUrl("https://aspia.org/help"));
}

void HostWindow::onAbout()
{
    common::AboutDialog(this).exec();
}

void HostWindow::createLanguageMenu(const QString& current_locale)
{
    QActionGroup* language_group = new QActionGroup(this);

    for (const auto& locale : locale_loader_.sortedLocaleList())
    {
        common::LanguageAction* action_language = new common::LanguageAction(locale, this);

        action_language->setActionGroup(language_group);
        action_language->setCheckable(true);

        if (current_locale == locale)
            action_language->setChecked(true);

        ui.menu_language->addAction(action_language);
    }
}

} // namespace host
