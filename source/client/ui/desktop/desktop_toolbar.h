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

#ifndef CLIENT_UI_DESKTOP_DESKTOP_TOOLBAR_H
#define CLIENT_UI_DESKTOP_DESKTOP_TOOLBAR_H

#include "proto/desktop_control.h"
#include "proto/desktop_power.h"
#include "proto/desktop_screen.h"
#include "proto/peer.h"
#include "ui_desktop_toolbar.h"

namespace client {

class DesktopToolBar final : public QFrame
{
    Q_OBJECT

public:
    explicit DesktopToolBar(QWidget* parent);
    ~DesktopToolBar() final;

    void enableScreenSelect(bool enable);
    void enablePowerControl(bool enable);
    void enableSystemInfo(bool enable);
    void enableTextChat(bool enable);
    void enableTaskManager(bool enable);
    void enableCtrlAltDelFeature(bool enable);
    void enablePasteAsKeystrokesFeature(bool enable);

    void setScreenList(const proto::screen::ScreenList& screen_list);
    void setSessionList(const proto::control::SessionList& session_list);
    void startRecording(bool enable);

    int scale() const { return scale_; }
    bool autoScrolling() const;
    bool sendKeyCombinations() const;
    bool isPanelHidden() const;
    bool isPanelPinned() const;
    bool isVideoPauseEnabled() const;
    bool isAudioPauseEnabled() const;

signals:
    void sig_pasteAsKeystrokes();
    void sig_keyCombination(QKeyCombination key_secuence);
    void sig_switchToFullscreen(bool fullscreen);
    void sig_switchToAutosize();
    void sig_settingsButton();
    void sig_screenSelected(const proto::screen::Screen& screen);
    void sig_scaleChanged();
    void sig_autoScrollChanged(bool enabled);
    void sig_keyCombinationsChanged(bool enabled);
    void sig_takeScreenshot();
    void sig_startSession(proto::peer::SessionType session_type);
    void sig_powerControl(proto::power::Control_Action action, bool wait);
    void sig_startTaskManager();
    void sig_startStatistics();
    void sig_minimizeSession();
    void sig_closeSession();
    void sig_showHidePanel();
    void sig_recordingStateChanged(bool enable);
    void sig_videoPauseChanged(bool enable);
    void sig_audioPauseChanged(bool enable);
    void sig_switchSession(quint32 session_id);

protected:
    // QFrame implementation.
    void enterEvent(QEnterEvent* event) final;
    void leaveEvent(QEvent* event) final;

private slots:
    void onHideTimer();
    void onFullscreenButton(bool checked);
    void onAutosizeButton();
    void onCtrlAltDel();
    void onPowerControl(QAction* action);
    void onChangeResolutionAction(QAction* action);
    void onChangeScreenAction(QAction* action);
    void onSwitchSessionAction(QAction* action);
    void onMenuShow();
    void onMenuHide();
    void onShowRecordSettings();

private:
    void createAdditionalMenu();
    void showFullScreenButtons(bool show);
    void updateScaleMenu();
    void updateSize();
    void delayedHide();

    Ui::DesktopToolBar ui;

    bool is_recording_started_ = false;

    QSize current_resolution_;
    qint64 current_screen_id_ = -1;
    int screen_count_ = 0;

    std::unique_ptr<QMenu> power_menu_;
    QMenu* additional_menu_ = nullptr;

    QList<QAction*> screen_actions_;

    QMenu* resolutions_menu_ = nullptr;
    QActionGroup* resolutions_group_ = nullptr;

    QMenu* scale_menu_ = nullptr;
    QActionGroup* scale_group_ = nullptr;

    QMenu* sessions_menu_ = nullptr;
    QActionGroup* sessions_group_ = nullptr;
    QList<QAction*> sessions_actions_;

    QTimer* hide_timer_ = nullptr;

    bool allow_hide_ = true;
    bool leaved_ = true;

    int scale_ = 100;

    Q_DISABLE_COPY_MOVE(DesktopToolBar)
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_DESKTOP_TOOLBAR_H
