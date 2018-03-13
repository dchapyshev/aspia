//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_panel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/desktop_panel.h"

#include <QMenu>

#include "client/ui/key_sequence_dialog.h"

namespace aspia {

DesktopPanel::DesktopPanel(QWidget* parent)
    : QFrame(parent)
{
    ui.setupUi(this);

    keys_menu_ = new QMenu(this);
    keys_menu_->addAction(ui.action_alt_tab);
    keys_menu_->addAction(ui.action_alt_shift_tab);
    keys_menu_->addAction(ui.action_printscreen);
    keys_menu_->addAction(ui.action_alt_printscreen);
    keys_menu_->addAction(ui.action_custom);

    connect(ui.button_settings, SIGNAL(pressed()), SIGNAL(settingsButton()));
    connect(ui.button_autosize, SIGNAL(pressed()), SIGNAL(autosizeButton()));
    connect(ui.button_full_screen, SIGNAL(pressed()), SIGNAL(fullscreenButton()));
    connect(ui.button_ctrl_alt_del, SIGNAL(pressed()), SLOT(onCtrlAltDelButton()));
    connect(ui.button_exit, SIGNAL(pressed()), SIGNAL(closeButton()));

    connect(ui.action_alt_tab, SIGNAL(triggered()), SLOT(onAltTabAction()));
    connect(ui.action_alt_shift_tab, SIGNAL(triggered()), SLOT(onAltShiftTabAction()));
    connect(ui.action_printscreen, SIGNAL(triggered()), SLOT(onPrintScreenAction()));
    connect(ui.action_alt_printscreen, SIGNAL(triggered()), SLOT(onAltPrintScreenAction()));
    connect(ui.action_custom, SIGNAL(triggered()), SLOT(onCustomAction()));

    ui.button_send_keys->setMenu(keys_menu_);
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

} // namespace aspia
