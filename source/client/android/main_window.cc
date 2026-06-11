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

#include "client/android/main_window.h"

#include <QEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "client/android/settings_widget.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "common/android/label.h"

namespace {

//--------------------------------------------------------------------------------------------------
QWidget* createPlaceholder(const QString& text)
{
    Label* label = new Label(text, Label::Role::CAPTION);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

} // namespace

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::AndroidMainWindow(QWidget* parent)
    : QWidget(parent),
      app_bar_(new AppBar(this)),
      content_(new QStackedWidget(this)),
      navigation_(new BottomNavigationBar(this))
{
    content_->addWidget(createPlaceholder(tr("Local")));
    content_->addWidget(createPlaceholder(tr("Remote")));
    content_->addWidget(createPlaceholder(tr("Routers")));
    content_->addWidget(new SettingsWidget(this));

    navigation_->addItem(tr("Local"), ":/img/folder.svg");
    navigation_->addItem(tr("Remote"), ":/img/workspace.svg");
    navigation_->addItem(tr("Routers"), ":/img/stack.svg");
    navigation_->addItem(tr("Settings"), ":/img/settings.svg");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(content_, 1);
    layout->addWidget(navigation_);

    connect(navigation_, &BottomNavigationBar::currentChanged,
            this, &AndroidMainWindow::onSectionChanged);

    onSectionChanged(navigation_->currentIndex());
}

//--------------------------------------------------------------------------------------------------
AndroidMainWindow::~AndroidMainWindow() = default;

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    // The language is switched from the settings combo box, and retranslation rebuilds that combo,
    // so the rebuild is deferred to avoid destroying the sender during its own signal.
    if (event->type() == QEvent::LanguageChange)
        QMetaObject::invokeMethod(this, [this]() { retranslate(); }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSectionChanged(int index)
{
    content_->setCurrentIndex(index);

    switch (index)
    {
        case 0:
            app_bar_->setTitle(tr("Local"));
            break;

        case 1:
            app_bar_->setTitle(tr("Remote"));
            break;

        case 2:
            app_bar_->setTitle(tr("Routers"));
            break;

        case 3:
            app_bar_->setTitle(tr("Settings"));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    navigation_->setItemText(0, tr("Local"));
    navigation_->setItemText(1, tr("Remote"));
    navigation_->setItemText(2, tr("Routers"));
    navigation_->setItemText(3, tr("Settings"));

    if (Label* label = qobject_cast<Label*>(content_->widget(0)))
        label->setText(tr("Local"));
    if (Label* label = qobject_cast<Label*>(content_->widget(1)))
        label->setText(tr("Remote"));
    if (Label* label = qobject_cast<Label*>(content_->widget(2)))
        label->setText(tr("Routers"));

    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(3)))
        settings->retranslate();

    onSectionChanged(navigation_->currentIndex());
}
