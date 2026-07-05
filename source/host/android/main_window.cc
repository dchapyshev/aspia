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

    content_->addWidget(new HomeWidget(this));
    content_->addWidget(new SettingsWidget(this));

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
    content_->setCurrentIndex(index);
    app_bar_->setTitle(sectionTitle(index));
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

    app_bar_->setTitle(sectionTitle(navigation_->currentIndex()));
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
