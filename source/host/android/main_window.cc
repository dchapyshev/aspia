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

#include <QApplication>
#include <QEvent>
#include <QInputMethod>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "common/android/app_bar.h"
#include "common/android/bottom_navigation_bar.h"
#include "host/database.h"
#include "host/android/connection_widget.h"
#include "host/android/password_dialog.h"
#include "host/android/server.h"
#include "host/android/settings_widget.h"
#include "proto/user.h"

namespace {

// Order of the pages in the stacked content and of the bottom navigation items.
enum Section
{
    SECTION_CONNECTION = 0,
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

    connection_ = new ConnectionWidget(this);
    content_->addWidget(connection_);
    content_->addWidget(settings);

    connect(app_bar_, &AppBar::sig_backClicked, this, &AndroidMainWindow::onBackClicked);

    navigation_->addItem(tr("Connection"), ":/img/computer.svg");
    navigation_->addItem(tr("Settings"), ":/img/settings.svg");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(app_bar_);
    layout->addWidget(content_, 1);
    layout->addWidget(navigation_);

    connect(navigation_, &BottomNavigationBar::sig_currentChanged,
            this, &AndroidMainWindow::onSectionChanged);

    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &AndroidMainWindow::onUpdateKeyboardInset);

    retranslate();
    onSectionChanged(navigation_->currentIndex());

    // Run the host server on the application I/O thread. Its credentials/router-state signals are
    // delivered here queued (cross-thread) and shown on the connection screen.
    server_ = new Server();
    connect(server_, &Server::sig_credentialsChanged,
            this, &AndroidMainWindow::onCredentialsChanged);
    connect(server_, &Server::sig_routerStateChanged,
            this, &AndroidMainWindow::onRouterStateChanged);
    server_->moveToThread(GuiApplication::ioThread());
    QMetaObject::invokeMethod(server_, &Server::start, Qt::QueuedConnection);
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
void AndroidMainWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // A key press on the on-screen keyboard can restore the full window height with the keyboard still
    // up, which arrives as a resize rather than a keyboard rectangle change; recompute the inset here.
    onUpdateKeyboardInset();
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onSectionChanged(int index)
{
    // Gate the settings tab behind the protection password, if one is set. On failure the tab switch
    // is reverted to the connection tab (which re-enters this slot without a gate).
    if (index == SECTION_SETTINGS && !settings_unlocked_)
    {
        if (Database::instance().passwordProtectionState() == Database::PasswordProtection::ENABLED &&
            !PasswordDialog::verify(this))
        {
            navigation_->setCurrentIndex(SECTION_CONNECTION);
            return;
        }

        settings_unlocked_ = true;
    }

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
void AndroidMainWindow::onUpdateKeyboardInset()
{
    const QInputMethod* input_method = QGuiApplication::inputMethod();
    const bool keyboard_visible = input_method->isVisible();

    QScrollArea* area = focusedScrollArea();

    int inset = 0;
    if (keyboard_visible && area)
    {
        // Overlap of the keyboard with the scroll viewport. Backing out the inset already applied gives
        // the viewport bottom Android itself settled on: zero when adjustResize resized the window above
        // the keyboard, positive when it left the window at full height (e.g. after a key press on the
        // on-screen keyboard). The keyboard rectangle is in physical pixels, so scale it down.
        const int keyboard_top = qRound(input_method->keyboardRectangle().top() / devicePixelRatioF());
        const QWidget* viewport = area->viewport();
        const int viewport_bottom = viewport->mapToGlobal(QPoint(0, viewport->height())).y();
        inset = qMax(0, viewport_bottom + keyboard_inset_ - keyboard_top);
    }

    // The bottom navigation would sit over the field the keyboard pushes up, so hide it while typing.
    navigation_->setVisible(!keyboard_visible);

    if (inset != keyboard_inset_)
    {
        keyboard_inset_ = inset;
        layout()->setContentsMargins(0, 0, 0, inset);
    }

    // Bring the focused field into the content area above the keyboard. Deferred so it runs after the
    // layout has settled around the new inset.
    if (keyboard_visible && area)
        QMetaObject::invokeMethod(this, &AndroidMainWindow::scrollFocusIntoView, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onCredentialsChanged(const QString& host_id, const QString& password)
{
    // The one-time password is only meaningful once the router has assigned an ID.
    const bool has_id = !host_id.isEmpty();

    connection_->setHostId(has_id ? host_id : QString("-"));
    connection_->setPassword((has_id && !password.isEmpty()) ? password : QString("-"));
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::onRouterStateChanged(int state, const QString& router)
{
    ConnectionWidget::RouterState mapped = ConnectionWidget::RouterState::DISABLED;

    switch (state)
    {
        case proto::user::RouterState::CONNECTING:
            mapped = ConnectionWidget::RouterState::CONNECTING;
            break;

        case proto::user::RouterState::CONNECTED:
            mapped = ConnectionWidget::RouterState::CONNECTED;
            break;

        case proto::user::RouterState::FAILED:
            mapped = ConnectionWidget::RouterState::FAILED;
            break;

        default:
            mapped = ConnectionWidget::RouterState::DISABLED;
            break;
    }

    connection_->setRouterState(mapped, router);
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::retranslate()
{
    navigation_->setItemText(SECTION_CONNECTION, tr("Connection"));
    navigation_->setItemText(SECTION_SETTINGS, tr("Settings"));

    if (ConnectionWidget* home = qobject_cast<ConnectionWidget*>(content_->widget(SECTION_CONNECTION)))
        home->retranslate();
    if (SettingsWidget* settings = qobject_cast<SettingsWidget*>(content_->widget(SECTION_SETTINGS)))
        settings->retranslate();

    onSectionChanged(navigation_->currentIndex());
}

//--------------------------------------------------------------------------------------------------
void AndroidMainWindow::scrollFocusIntoView()
{
    QScrollArea* area = focusedScrollArea();
    if (!area)
        return;

    QWidget* focus = QApplication::focusWidget();
    if (!focus)
        return;

    // Place the focused field just above the bottom of the viewport. Scrolling by the exact overlap
    // keeps the field flush above the keyboard in both directions, so a field left too high is pulled
    // back down instead of leaving a gap.
    static const int kMargin = 16;
    const QWidget* viewport = area->viewport();
    const int viewport_bottom = viewport->mapToGlobal(QPoint(0, viewport->height())).y();
    const int field_bottom = focus->mapToGlobal(QPoint(0, focus->height())).y();
    const int delta = field_bottom + kMargin - viewport_bottom;
    if (delta == 0)
        return;

    QScrollBar* bar = area->verticalScrollBar();
    bar->setValue(bar->value() + delta);
}

//--------------------------------------------------------------------------------------------------
QScrollArea* AndroidMainWindow::focusedScrollArea() const
{
    QWidget* focus = QApplication::focusWidget();
    if (!focus)
        return nullptr;

    // The scroll area enclosing the focused field, but only within this window: a modal dialog on top
    // has its own scroll area and manages its own keyboard, so it must not drive the inset here.
    for (QWidget* widget = focus->parentWidget(); widget; widget = widget->parentWidget())
    {
        if (QScrollArea* area = qobject_cast<QScrollArea*>(widget))
            return isAncestorOf(area) ? area : nullptr;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QString AndroidMainWindow::sectionTitle(int index) const
{
    switch (index)
    {
        case SECTION_CONNECTION:
            return tr("Connection");
        case SECTION_SETTINGS:
            return tr("Settings");
        default:
            return QString();
    }
}
