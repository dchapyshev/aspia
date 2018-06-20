//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_panel.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_PANEL_H
#define _ASPIA_CLIENT__UI__DESKTOP_PANEL_H

#include <QPointer>

#include "protocol/authorization.pb.h"
#include "ui_desktop_panel.h"

namespace aspia {

class DesktopPanel : public QFrame
{
    Q_OBJECT

public:
    DesktopPanel(proto::auth::SessionType session_type, QWidget* parent);
    ~DesktopPanel() = default;

signals:
    void keySequence(int key_secuence);
    void switchToFullscreen(bool fullscreen);
    void switchToAutosize();
    void settingsButton();

protected:
    // QFrame implementation.
    void timerEvent(QTimerEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onFullscreenButton(bool checked);
    void onAutosizeButton();
    void onCtrlAltDelButton();
    void onAltTabAction();
    void onAltShiftTabAction();
    void onPrintScreenAction();
    void onAltPrintScreenAction();
    void onCustomAction();

private:
    void delayedHide();

    Ui::DesktopPanel ui;
    QPointer<QMenu> keys_menu_;
    int hide_timer_id_ = 0;

    bool allow_hide_ = true;
    bool leaved_ = true;

    Q_DISABLE_COPY(DesktopPanel)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_PANEL_H
