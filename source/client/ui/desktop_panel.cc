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

#include "client/ui/desktop_panel.h"

#include <QMenu>
#include <QToolButton>

#include "client/ui/key_sequence_dialog.h"
#include "client/ui/select_screen_action.h"

namespace aspia {

DesktopPanel::DesktopPanel(proto::auth::SessionType session_type, QWidget* parent)
    : QFrame(parent)
{
    ui.setupUi(this);

    connect(ui.action_settings, &QAction::triggered, this, &DesktopPanel::settingsButton);
    connect(ui.action_autosize, &QAction::triggered, this, &DesktopPanel::onAutosizeButton);
    connect(ui.action_fullscreen, &QAction::triggered, this, &DesktopPanel::onFullscreenButton);
    connect(ui.action_autoscroll, &QAction::triggered, this, &DesktopPanel::autoScrollChanged);

    screens_menu_ = new QMenu(this);
    screens_group_ = new QActionGroup(this);

    SelectScreenAction* full_screen_action = new SelectScreenAction(screens_group_);
    screens_group_->addAction(full_screen_action);
    screens_menu_->addAction(full_screen_action);

    ui.action_monitors->setMenu(screens_menu_);

    QToolButton* button_monitors =
        qobject_cast<QToolButton*>(ui.toolbar->widgetForAction(ui.action_monitors));
    button_monitors->setPopupMode(QToolButton::InstantPopup);

    connect(screens_menu_, &QMenu::aboutToShow, [this]() { allow_hide_ = false; });
    connect(screens_menu_, &QMenu::aboutToHide, [this]()
    {
        allow_hide_ = true;

        if (leaved_)
            delayedHide();
    });

    if (session_type == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        keys_menu_ = new QMenu(this);
        keys_menu_->addAction(ui.action_alt_tab);
        keys_menu_->addAction(ui.action_alt_shift_tab);
        keys_menu_->addAction(ui.action_printscreen);
        keys_menu_->addAction(ui.action_alt_printscreen);
        keys_menu_->addAction(ui.action_custom);

        connect(keys_menu_, &QMenu::aboutToShow, [this]() { allow_hide_ = false; });
        connect(keys_menu_, &QMenu::aboutToHide, [this]()
        {
            allow_hide_ = true;

            if (leaved_)
                delayedHide();
        });

        connect(ui.action_cad, &QAction::triggered,
                this, &DesktopPanel::onCtrlAltDelButton);

        connect(ui.action_alt_tab, &QAction::triggered,
                this, &DesktopPanel::onAltTabAction);

        connect(ui.action_alt_shift_tab, &QAction::triggered,
                this, &DesktopPanel::onAltShiftTabAction);

        connect(ui.action_printscreen, &QAction::triggered,
                this, &DesktopPanel::onPrintScreenAction);

        connect(ui.action_alt_printscreen, &QAction::triggered,
                this, &DesktopPanel::onAltPrintScreenAction);

        connect(ui.action_custom, &QAction::triggered,
                this, &DesktopPanel::onCustomAction);

        ui.action_send_keys->setMenu(keys_menu_);

        QToolButton* button_send_keys =
            qobject_cast<QToolButton*>(ui.toolbar->widgetForAction(ui.action_send_keys));
        button_send_keys->setPopupMode(QToolButton::InstantPopup);
    }
    else
    {
        Q_ASSERT(session_type == proto::auth::SESSION_TYPE_DESKTOP_VIEW);

        ui.action_send_keys->setVisible(false);
        ui.action_cad->setVisible(false);
    }

    ui.frame->hide();
    adjustSize();

    hide_timer_id_ = startTimer(std::chrono::seconds(1));
}

void DesktopPanel::setScreenList(const proto::desktop::ScreenList& screen_list)
{
    delete screens_group_;

    screens_group_ = new QActionGroup(this);

    connect(screens_group_, &QActionGroup::triggered, [this](QAction* action)
    {
        SelectScreenAction* screen_action = dynamic_cast<SelectScreenAction*>(action);
        if (!screen_action)
            return;

        emit screenSelected(screen_action->screen());
    });

    SelectScreenAction* full_desktop_action = new SelectScreenAction(screens_group_);

    screens_group_->addAction(full_desktop_action);
    screens_menu_->addAction(full_desktop_action);

    for (int i = 0; i < screen_list.screen_size(); ++i)
    {
        SelectScreenAction* action = new SelectScreenAction(screen_list.screen(i), screens_group_);

        screens_group_->addAction(action);
        screens_menu_->addAction(action);
    }

    ui.action_monitors->setEnabled(true);
}

void DesktopPanel::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == hide_timer_id_)
    {
        killTimer(hide_timer_id_);
        hide_timer_id_ = 0;

        ui.frame->setFixedWidth(ui.toolbar->width());

        ui.toolbar->hide();
        ui.frame->show();

        adjustSize();
        return;
    }

    QFrame::timerEvent(event);
}

void DesktopPanel::enterEvent(QEvent* event)
{
    leaved_ = false;

    if (allow_hide_)
    {
        if (hide_timer_id_)
        {
            killTimer(hide_timer_id_);
            hide_timer_id_ = 0;
        }

        ui.toolbar->show();
        ui.frame->hide();

        adjustSize();
    }

    QFrame::enterEvent(event);
}

void DesktopPanel::leaveEvent(QEvent* event)
{
    leaved_ = true;

    if (allow_hide_)
        delayedHide();

    QFrame::leaveEvent(event);
}

void DesktopPanel::onFullscreenButton(bool checked)
{
    if (checked)
    {
        ui.action_fullscreen->setIcon(
            QIcon(QStringLiteral(":/icon/application-resize-actual.png")));
    }
    else
    {
        ui.action_fullscreen->setIcon(
            QIcon(QStringLiteral(":/icon/application-resize-full.png")));
    }

    emit switchToFullscreen(checked);
}

void DesktopPanel::onAutosizeButton()
{
    if (ui.action_fullscreen->isChecked())
    {
        ui.action_fullscreen->setIcon(
            QIcon(QStringLiteral(":/icon/application-resize-full.png")));
        ui.action_fullscreen->setChecked(false);
    }

    emit switchToAutosize();
}

void DesktopPanel::onCtrlAltDelButton()
{
    emit keySequence(Qt::ControlModifier | Qt::AltModifier | Qt::Key_Delete);
}

void DesktopPanel::onAltTabAction()
{
    emit keySequence(Qt::AltModifier | Qt::Key_Tab);
}

void DesktopPanel::onAltShiftTabAction()
{
    emit keySequence(Qt::AltModifier | Qt::ShiftModifier | Qt::Key_Tab);
}

void DesktopPanel::onPrintScreenAction()
{
    emit keySequence(Qt::Key_Print);
}

void DesktopPanel::onAltPrintScreenAction()
{
    emit keySequence(Qt::AltModifier | Qt::Key_Print);
}

void DesktopPanel::onCustomAction()
{
    QKeySequence key_sequence = KeySequenceDialog::keySequence(this);

    for (int i = 0; i < key_sequence.count(); ++i)
        emit keySequence(key_sequence[i]);
}

void DesktopPanel::delayedHide()
{
    if (!ui.action_pin->isChecked() && !hide_timer_id_)
        hide_timer_id_ = startTimer(std::chrono::seconds(1));
}

} // namespace aspia
