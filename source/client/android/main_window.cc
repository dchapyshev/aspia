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

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "client/android/master_password_dialog.h"
#include "client/android/remote_widget.h"
#include "client/android/routers_widget.h"
#include "client/android/settings_widget.h"
#include "client/master_password.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "common/android/label.h"

namespace {

// Order of the pages in the stacked content and of the bottom navigation items.
enum Section
{
    SECTION_LOCAL = 0,
    SECTION_REMOTE,
    SECTION_ROUTERS,
    SECTION_SETTINGS
};

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
    RoutersWidget* routers = new RoutersWidget(this);
    RemoteWidget* remote = new RemoteWidget(this);

    content_->addWidget(createPlaceholder(tr("Local")));
    content_->addWidget(remote);
    content_->addWidget(routers);
    content_->addWidget(new SettingsWidget(this));

    connect(routers, &RoutersWidget::appBarActionsChanged,
            this, &AndroidMainWindow::onRouterActionsChanged);
    connect(remote, &RemoteWidget::sig_titleChanged,
            this, &AndroidMainWindow::onRemoteTitleChanged);
    connect(app_bar_, &AppBar::backClicked, remote, &RemoteWidget::goBack);

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

    // The gate runs once the event loop is active so the window is laid out and the dialog scrim
    // covers it.
    QMetaObject::invokeMethod(this, [this]() { runMasterPasswordGate(); }, Qt::QueuedConnection);
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

    // The back button belongs to the remote host view; any tab switch clears it.
    app_bar_->setBackVisible(false);

    RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS));
    if (index != SECTION_ROUTERS && routers)
        routers->resetEditMode();
    app_bar_->setActions((index == SECTION_ROUTERS && routers) ? routers->appBarActions()
                                                               : QList<QWidget*>());

    // Pick up routers added or removed elsewhere when the remote screen becomes visible.
    if (index == SECTION_REMOTE)
    {
        if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE)))
            remote->reload();
    }

    switch (index)
    {
        case SECTION_LOCAL:
            app_bar_->setTitle(tr("Local"));
            break;

        case SECTION_REMOTE:
            app_bar_->setTitle(tr("Remote"));
            break;

        case SECTION_ROUTERS:
            app_bar_->setTitle(tr("Routers"));
            break;

        case SECTION_SETTINGS:
            app_bar_->setTitle(tr("Settings"));
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRouterActionsChanged()
{
    RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS));
    if (navigation_->currentIndex() == SECTION_ROUTERS && routers)
        app_bar_->setActions(routers->appBarActions());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRemoteTitleChanged(const QString& title, bool back_visible)
{
    if (navigation_->currentIndex() != SECTION_REMOTE)
        return;

    app_bar_->setTitle(title.isEmpty() ? tr("Remote") : title);
    app_bar_->setBackVisible(back_visible);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::runMasterPasswordGate()
{
    const MasterPasswordDialog::Mode mode = MasterPassword::isSet() ?
        MasterPasswordDialog::Mode::UNLOCK : MasterPasswordDialog::Mode::CREATE;

    // The data cryptor is invalid until the dialog unlocks it, so quit if the user cancels.
    MasterPasswordDialog dialog(mode, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        QCoreApplication::quit();
        return;
    }

    onUnlocked();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onUnlocked()
{
    if (RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS)))
        routers->reload();

    if (RemoteWidget* remote = qobject_cast<RemoteWidget*>(content_->widget(SECTION_REMOTE)))
        remote->reload();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    navigation_->setItemText(SECTION_LOCAL, tr("Local"));
    navigation_->setItemText(SECTION_REMOTE, tr("Remote"));
    navigation_->setItemText(SECTION_ROUTERS, tr("Routers"));
    navigation_->setItemText(SECTION_SETTINGS, tr("Settings"));

    if (Label* label = qobject_cast<Label*>(content_->widget(SECTION_LOCAL)))
        label->setText(tr("Local"));

    if (RoutersWidget* routers = qobject_cast<RoutersWidget*>(content_->widget(SECTION_ROUTERS)))
        routers->retranslate();

    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
        settings->retranslate();

    onSectionChanged(navigation_->currentIndex());
}
