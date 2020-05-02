//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT__UI__DESKTOP_PANEL_H
#define CLIENT__UI__DESKTOP_PANEL_H

#include "base/macros_magic.h"
#include "client/ui/desktop_settings.h"
#include "proto/common.pb.h"
#include "proto/desktop.pb.h"
#include "proto/desktop_extensions.pb.h"
#include "ui_desktop_panel.h"

namespace client {

class DesktopPanel : public QFrame
{
    Q_OBJECT

public:
    DesktopPanel(proto::SessionType session_type, QWidget* parent);
    ~DesktopPanel();

    void enableScreenSelect(bool enable);
    void enablePowerControl(bool enable);
    void enableSystemInfo(bool enable);
    void enableRemoteUpdate(bool enable);

    void setScreenList(const proto::ScreenList& screen_list);

    int scale() const { return scale_; }
    bool autoScrolling() const;
    bool sendKeyCombinations() const;

signals:
    void keyCombination(int key_secuence);
    void switchToFullscreen(bool fullscreen);
    void switchToAutosize();
    void settingsButton();
    void screenSelected(const proto::Screen& screen);
    void scaleChanged();
    void autoScrollChanged(bool enabled);
    void keyCombinationsChanged(bool enabled);
    void takeScreenshot();
    void startSession(proto::SessionType session_type);
    void powerControl(proto::PowerControl::Action action);
    void startRemoteUpdate();
    void startSystemInfo();
    void startStatistic();
    void minimizeSession();
    void closeSession();

protected:
    // QFrame implementation.
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onHideTimer();
    void onFullscreenButton(bool checked);
    void onAutosizeButton();
    void onCtrlAltDel();
    void onPowerControl(QAction* action);

private:
    void createAdditionalMenu(proto::SessionType session_type);
    void showFullScreenButtons(bool show);
    void updateScaleMenu();
    void updateSize();
    void delayedHide();

    Ui::DesktopPanel ui;

    const proto::SessionType session_type_;

    DesktopSettings settings_;

    QScopedPointer<QMenu> power_menu_;
    QMenu* additional_menu_ = nullptr;

    QMenu* scale_menu_ = nullptr;
    QActionGroup* scale_group_ = nullptr;

    QScopedPointer<QMenu> screens_menu_;
    QActionGroup* screens_group_ = nullptr;

    QTimer* hide_timer_ = nullptr;

    bool allow_hide_ = true;
    bool leaved_ = true;

    int scale_ = 100;

    DISALLOW_COPY_AND_ASSIGN(DesktopPanel);
};

} // namespace client

#endif // CLIENT__UI__DESKTOP_PANEL_H
