//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/android/main_window.h"

#include <QEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/logging.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "host/android/home_widget.h"
#include "host/android/settings_widget.h"

namespace {

// Order of the pages in the stacked content and of the bottom navigation items.
enum Section
{
    SECTION_HOME = 0,
    SECTION_SETTINGS
};

} // namespace

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::AndroidMainWindow(QWidget* parent)
    : QWidget(parent),
      app_bar_(new AppBar(this)),
      content_(new QStackedWidget(this)),
      navigation_(new BottomNavigationBar(this))
{
    LOG(INFO) << "Ctor";

    SettingsWidget* settings = new SettingsWidget(this);
    connect(settings, &SettingsWidget::sig_titleChanged,
            this, &AndroidMainWindow::onSettingsTitleChanged);
    connect(settings, &SettingsWidget::sig_appBarActionsChanged,
            this, &AndroidMainWindow::onSettingsActionsChanged);

    content_->addWidget(new HomeWidget(this));
    content_->addWidget(settings);

    connect(app_bar_, &AppBar::sig_backClicked, this, &AndroidMainWindow::onBackClicked);

    navigation_->addItem(tr("Home"), ":/img/home.svg");
    navigation_->addItem(tr("Settings"), ":/img/settings.svg");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(content_, 1);
    layout->addWidget(navigation_);

    connect(navigation_, &BottomNavigationBar::sig_currentChanged,
            this, &AndroidMainWindow::onSectionChanged);

    retranslate();
    onSectionChanged(navigation_->currentIndex());
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    // The language is switched from the settings combo box, and retranslation rebuilds that combo, so
    // the rebuild is deferred to avoid destroying the sender during its own signal.
    if (event->type() == QEvent::LanguageChange)
        QMetaObject::invokeMethod(this, [this]() { retranslate(); }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSectionChanged(int index)
{
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS));

    // Leaving the settings tab returns its sub-page (about) to the settings page.
    if (index != SECTION_SETTINGS && settings)
        settings->resetToSettings();

    content_->setCurrentIndex(index);

    app_bar_->setBackVisible(false);
    app_bar_->setTitle(sectionTitle(index));

    // The about action belongs to the settings tab; show it only there.
    app_bar_->setActions((index == SECTION_SETTINGS && settings) ? settings->appBarActions()
                                                                 : QList<QWidget*>());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_SETTINGS)
        return;

    app_bar_->setTitle(title.isEmpty() ? sectionTitle(SECTION_SETTINGS) : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSettingsActionsChanged()
{
    SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS));
    if (navigation_->currentIndex() == SECTION_SETTINGS && settings)
        app_bar_->setActions(settings->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onBackClicked()
{
    if (navigation_->currentIndex() != SECTION_SETTINGS)
        return;

    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
        settings->goBack();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    navigation_->setItemText(SECTION_HOME, tr("Home"));
    navigation_->setItemText(SECTION_SETTINGS, tr("Settings"));

    if (HomeWidget* home = qobject_cast<HomeWidget*>(content_->widget(SECTION_HOME)))
        home->retranslate();
    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
        settings->retranslate();

    onSectionChanged(navigation_->currentIndex());
}

//--------------------------------------------------------------------------------------------------
QString AndroidMainWindow::sectionTitle(int index) const
{
    switch (index)
    {
        case SECTION_HOME:
            return tr("Home");
        case SECTION_SETTINGS:
            return tr("Settings");
        default:
            return QString();
    }
}
