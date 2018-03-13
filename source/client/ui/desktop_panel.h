//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_panel.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_PANEL_H
#define _ASPIA_CLIENT__UI__DESKTOP_PANEL_H

#include "base/macros.h"
#include "qt/ui_desktop_panel.h"

namespace aspia {

class DesktopPanel : public QFrame
{
    Q_OBJECT

public:
    DesktopPanel(QWidget* parent);
    ~DesktopPanel() = default;

signals:
    void keySequence(int key_secuence);
    void settingsButton();
    void autosizeButton();
    void fullscreenButton();
    void closeButton();

private slots:
    void onCtrlAltDelButton();
    void onAltTabAction();
    void onAltShiftTabAction();
    void onPrintScreenAction();
    void onAltPrintScreenAction();
    void onCustomAction();

private:
    Ui::DesktopPanel ui;
    QMenu* keys_menu_;

    DISALLOW_COPY_AND_ASSIGN(DesktopPanel);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_PANEL_H
