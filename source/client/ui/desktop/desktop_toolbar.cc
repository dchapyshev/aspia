//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/desktop/desktop_toolbar.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "client/ui/desktop/desktop_settings.h"
#include "client/ui/desktop/record_settings_dialog.h"
#include "client/ui/desktop/select_screen_action.h"

#include <QActionGroup>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QToolButton>

namespace client {

//--------------------------------------------------------------------------------------------------
DesktopToolBar::DesktopToolBar(proto::SessionType session_type, QWidget* parent)
    : QFrame(parent),
      session_type_(session_type)
{
    LOG(LS_INFO) << "Ctor";

    ui.setupUi(this);

    hide_timer_ = new QTimer(this);
    connect(hide_timer_, &QTimer::timeout, this, &DesktopToolBar::onHideTimer);

    resolutions_menu_ = new QMenu(this);
    resolutions_group_ = new QActionGroup(resolutions_menu_);

    connect(resolutions_menu_, &QMenu::aboutToShow, this, &DesktopToolBar::onMenuShow);
    connect(resolutions_menu_, &QMenu::aboutToHide, this, &DesktopToolBar::onMenuHide);

    connect(resolutions_group_, &QActionGroup::triggered, this, [this](QAction* action)
    {
        onChangeResolutionAction(action);
    });

    DesktopSettings settings;

    ui.action_autoscroll->setChecked(settings.autoScrolling());

    scale_ = settings.scale();

    // Sending key combinations is available only in desktop management.
    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
        ui.action_send_key_combinations->setChecked(settings.sendKeyCombinations());
    else
        ui.action_send_key_combinations->setChecked(false);

    ui.action_pause_video->setChecked(settings.pauseVideoWhenMinimizing());
    ui.action_pause_audio->setChecked(settings.pauseAudioWhenMinimizing());

    connect(ui.action_settings, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Settings button clicked";
        emit sig_settingsButton();
    });

    connect(ui.action_autosize, &QAction::triggered, this, &DesktopToolBar::onAutosizeButton);
    connect(ui.action_fullscreen, &QAction::triggered, this, &DesktopToolBar::onFullscreenButton);
    connect(ui.action_autoscroll, &QAction::triggered, this, [this](bool enabled)
    {
        LOG(LS_INFO) << "[ACTION] Auto-scroll changed: " << enabled;
        emit sig_autoScrollChanged(enabled);
    });
    connect(ui.action_update, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Remote update requested";
        emit sig_startRemoteUpdate();
    });
    connect(ui.action_system_info, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] System info requested";
        emit sig_startSystemInfo();
    });
    connect(ui.action_task_manager, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Task manager requested";
        emit sig_startTaskManager();
    });
    connect(ui.action_statistics, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Statistics requested";
        emit sig_startStatistics();
    });
    connect(ui.action_minimize, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Minimize session";
        emit sig_minimizeSession();
    });
    connect(ui.action_close, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Close session";
        emit sig_closeSession();
    });

    createAdditionalMenu(session_type);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        connect(ui.action_cad, &QAction::triggered, this, &DesktopToolBar::onCtrlAltDel);
    }
    else
    {
        DCHECK(session_type == proto::SESSION_TYPE_DESKTOP_VIEW);
        ui.action_cad->setVisible(false);
    }

    connect(ui.action_file_transfer, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] File transfer requested";
        emit sig_startSession(proto::SESSION_TYPE_FILE_TRANSFER);
    });

    connect(ui.action_text_chat, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Text chat requested";
        emit sig_startSession(proto::SESSION_TYPE_TEXT_CHAT);
    });

    connect(ui.action_port_forwarding, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Port forwarding requested";
        emit sig_startSession(proto::SESSION_TYPE_PORT_FORWARDING);
    });

    bool is_pinned = settings.isToolBarPinned();

    ui.action_pin->setChecked(is_pinned);
    ui.toolbar->setVisible(is_pinned);
    ui.frame->setVisible(!is_pinned);

    if (!is_pinned)
        hide_timer_->start(0);

#if defined(Q_OS_MACOS)
    // MacOS has its own button to maximize the window to full screen.
    ui.action_fullscreen->setVisible(false);
#endif // defined(Q_OS_MACOS)

    showFullScreenButtons(false);
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
DesktopToolBar::~DesktopToolBar()
{
    LOG(LS_INFO) << "Dtor";

    DesktopSettings settings;
    settings.setScale(scale_);
    settings.setAutoScrolling(ui.action_autoscroll->isChecked());
    settings.setToolBarPinned(ui.action_pin->isChecked());
    settings.setPauseVideoWhenMinimizing(ui.action_pause_video->isChecked());
    settings.setPauseAudioWhenMinimizing(ui.action_pause_audio->isChecked());

    // Save the parameter only for desktop management.
    if (session_type_ == proto::SESSION_TYPE_DESKTOP_MANAGE)
        settings.setSendKeyCombinations(ui.action_send_key_combinations->isChecked());
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableScreenSelect(bool enable)
{
    LOG(LS_INFO) << "enableScreenSelect: " << enable;
    // By default, we disable the monitor selection menu. Selection will be enabled when receiving
    // a list of monitors.
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enablePowerControl(bool enable)
{
    LOG(LS_INFO) << "enablePowerControl: " << enable;

    ui.action_power_control->setVisible(enable);
    ui.action_power_control->setEnabled(enable);

    if (!enable)
    {
        power_menu_.reset();
    }
    else
    {
        power_menu_.reset(new QMenu());
        power_menu_->addAction(ui.action_shutdown);
        power_menu_->addAction(ui.action_reboot);
        power_menu_->addAction(ui.action_reboot_safe_mode);
        power_menu_->addSeparator();
        power_menu_->addAction(ui.action_logoff);
        power_menu_->addAction(ui.action_lock);

        ui.action_power_control->setMenu(power_menu_.get());

        QToolButton* button = qobject_cast<QToolButton*>(
            ui.toolbar->widgetForAction(ui.action_power_control));
        button->setPopupMode(QToolButton::InstantPopup);

        connect(power_menu_.get(), &QMenu::triggered, this, &DesktopToolBar::onPowerControl);
        connect(power_menu_.get(), &QMenu::aboutToShow, this, &DesktopToolBar::onMenuShow);
        connect(power_menu_.get(), &QMenu::aboutToHide, this, &DesktopToolBar::onMenuHide);
    }

    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableSystemInfo(bool enable)
{
    LOG(LS_INFO) << "enableSystemInfo: " << enable;
    ui.action_system_info->setVisible(enable);
    ui.action_system_info->setEnabled(enable);
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableTextChat(bool enable)
{
    LOG(LS_INFO) << "enableTextChat: " << enable;
    ui.action_text_chat->setVisible(enable);
    ui.action_text_chat->setEnabled(enable);
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableRemoteUpdate(bool enable)
{
    LOG(LS_INFO) << "enableRemoteUpdate: " << enable;
    is_remote_update_enabled_ = enable;
    ui.action_update->setVisible(enable);
    ui.action_update->setEnabled(enable);
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableTaskManager(bool enable)
{
    LOG(LS_INFO) << "enableTaskManager: " << enable;
    ui.action_task_manager->setVisible(enable);
    ui.action_task_manager->setEnabled(enable);
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableVideoPauseFeature(bool enable)
{
    LOG(LS_INFO) << "enableVideoPauseFeature: " << enable;
    ui.action_pause_video->setVisible(enable);
    ui.action_pause_video->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableAudioPauseFeature(bool enable)
{
    LOG(LS_INFO) << "enableAudioPauseFeature: " << enable;
    ui.action_pause_audio->setVisible(enable);
    ui.action_pause_audio->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enableCtrlAltDelFeature(bool enable)
{
    LOG(LS_INFO) << "enableCtrlAltDelFeature: " << enable;
    ui.action_cad->setVisible(enable);
    ui.action_cad->setEnabled(enable);
    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::enablePasteAsKeystrokesFeature(bool enable)
{
    LOG(LS_INFO) << "enablePasteAsKeystrokesFeature: " << enable;
    ui.action_paste_clipboard_as_keystrokes->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::setScreenList(const proto::desktop::ScreenList& screen_list)
{
    LOG(LS_INFO) << "Setting up a new list of screens (screens: " << screen_list.screen_size()
                 << " resolutions: " << screen_list.resolution_size() << ")";

    for (int i = 0; i < screen_actions_.size(); ++i)
        delete screen_actions_[i];
    screen_actions_.clear();
    resolutions_menu_->clear();

    current_screen_id_ = screen_list.current_screen();
    current_resolution_ = QSize(0, 0);
    screen_count_ = screen_list.screen_size();

    for (int i = 0; i < screen_list.screen_size(); ++i)
    {
        const proto::desktop::Screen& screen = screen_list.screen(i);

        if (screen_list.current_screen() == screen.id())
        {
            current_resolution_.setWidth(screen.resolution().width());
            current_resolution_.setHeight(screen.resolution().height());
        }
    }

    bool is_full_desktop = false;

    // If it has only one screen or an empty list is received.
    if (screen_list.screen_size() > 1)
    {
        for (int i = 0; i < screen_list.screen_size(); ++i)
        {
            const proto::desktop::Screen& screen = screen_list.screen(i);
            bool is_primary = screen.id() == screen_list.primary_screen();

            SelectScreenAction* action =
                new SelectScreenAction(i + 1, screen, is_primary, ui.toolbar);

            ui.toolbar->insertAction(ui.action_power_control, action);
            screen_actions_.append(action);

            if (screen_list.current_screen() == screen.id())
            {
                action->setChecked(true);

                if (screen_list.resolution_size() > 0)
                {
                    action->setMenu(resolutions_menu_);

                    QToolButton* button = qobject_cast<QToolButton*>(
                        ui.toolbar->widgetForAction(action));
                    if (button)
                        button->setPopupMode(QToolButton::InstantPopup);
                }
            }

            connect(action, &SelectScreenAction::triggered, this, [this, action]()
            {
                onChangeScreenAction(action);
            });
        }

        SelectScreenAction* full_desktop_action = new SelectScreenAction(ui.toolbar);
        if (screen_list.current_screen() == -1)
        {
            full_desktop_action->setChecked(true);
            is_full_desktop = true;
        }

        connect(full_desktop_action, &SelectScreenAction::triggered, this, [this, full_desktop_action]()
        {
            onChangeScreenAction(full_desktop_action);
        });

        ui.toolbar->insertAction(ui.action_power_control, full_desktop_action);
        screen_actions_.append(full_desktop_action);
    }
    else
    {
        QAction* resolution_select_action = new QAction(ui.toolbar);

        resolution_select_action->setToolTip(tr("Resolution selection"));
        resolution_select_action->setIcon(QIcon(":/img/monitor.png"));
        resolution_select_action->setMenu(resolutions_menu_);

        ui.toolbar->insertAction(ui.action_power_control, resolution_select_action);
        screen_actions_.append(resolution_select_action);

        QToolButton* button = qobject_cast<QToolButton*>(
            ui.toolbar->widgetForAction(resolution_select_action));
        if (button)
            button->setPopupMode(QToolButton::InstantPopup);

        if (screen_list.resolution_size() < 1)
            resolution_select_action->setVisible(false);
    }

    if (screen_list.resolution_size() > 0 && !is_full_desktop)
    {
        for (int i = 0; i < screen_list.resolution_size(); ++i)
        {
            QSize resolution =
                QSize(screen_list.resolution(i).width(), screen_list.resolution(i).height());

            QAction* resolution_action = new QAction();
            resolution_action->setText(
                QString("%1x%2").arg(resolution.width()).arg(resolution.height()));
            resolution_action->setData(resolution);
            resolution_action->setCheckable(true);
            if (resolution == current_resolution_)
                resolution_action->setChecked(true);

            resolutions_group_->addAction(resolution_action);
            resolutions_menu_->addAction(resolution_action);
        }
    }

    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::setScreenType(const proto::desktop::ScreenType& screen_type)
{
    if (is_remote_update_enabled_)
    {
        bool show_update_button = screen_type.type() == proto::desktop::ScreenType::TYPE_DESKTOP;

        LOG(LS_INFO) << "Show update button: " << show_update_button
                     << " (type=" << screen_type.type() << " name=" << screen_type.name() << ")";

        ui.action_update->setVisible(show_update_button);
        updateSize();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::startRecording(bool enable)
{
    LOG(LS_INFO) << "[ACTION] Start recording: " << enable;

    if (enable)
    {
        ui.action_start_recording->setIcon(QIcon(":/img/control-stop.png"));
        ui.action_start_recording->setText(tr("Stop recording"));
    }
    else
    {
        ui.action_start_recording->setIcon(QIcon(":/img/control-record.png"));
        ui.action_start_recording->setText(tr("Start recording"));
    }

    is_recording_started_ = enable;
    emit sig_recordingStateChanged(enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::autoScrolling() const
{
    return ui.action_autoscroll->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::sendKeyCombinations() const
{
    return ui.action_send_key_combinations->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::isPanelHidden() const
{
    return ui.toolbar->isHidden();
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::isPanelPinned() const
{
    return ui.action_pin->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::isVideoPauseEnabled() const
{
    return ui.action_pause_video->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool DesktopToolBar::isAudioPauseEnabled() const
{
    return ui.action_pause_audio->isChecked();
}

//--------------------------------------------------------------------------------------------------
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void DesktopToolBar::enterEvent(QEvent* /* event */)
#else
void DesktopToolBar::enterEvent(QEnterEvent* /* event */)
#endif
{
    leaved_ = false;

    if (hide_timer_->isActive())
        hide_timer_->stop();

    if (allow_hide_)
    {
        if (ui.toolbar->isHidden())
        {
            ui.toolbar->show();
            ui.frame->hide();

            emit sig_showHidePanel();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::leaveEvent(QEvent* /* event */)
{
    leaved_ = true;

    if (allow_hide_)
        delayedHide();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onHideTimer()
{
    hide_timer_->stop();

    ui.frame->setFixedWidth(50);
    ui.toolbar->hide();
    ui.frame->show();

    emit sig_showHidePanel();
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onFullscreenButton(bool checked)
{
    LOG(LS_INFO) << "[ACTION] Full screen button clicked: " << checked;

    if (checked)
        ui.action_fullscreen->setIcon(QIcon(":/img/application-resize-actual.png"));
    else
        ui.action_fullscreen->setIcon(QIcon(":/img/application-resize-full.png"));

    showFullScreenButtons(checked);

    emit sig_switchToFullscreen(checked);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onAutosizeButton()
{
    LOG(LS_INFO) << "[ACTION] Autosize button clicked";

    if (ui.action_fullscreen->isChecked())
    {
        ui.action_fullscreen->setIcon(QIcon(":/img/application-resize-full.png"));
        ui.action_fullscreen->setChecked(false);

        showFullScreenButtons(false);
    }

    emit sig_switchToAutosize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onCtrlAltDel()
{
    LOG(LS_INFO) << "[ACTION] Ctrl+Alt+Del button clicked";
    emit sig_keyCombination(Qt::ControlModifier | Qt::AltModifier | Qt::Key_Delete);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onPowerControl(QAction* action)
{
    if (action == ui.action_shutdown)
    {
        LOG(LS_INFO) << "[ACTION] Shutdown";
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you sure you want to shutdown the remote computer?"),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Shutdown accepted by user";
            emit sig_powerControl(proto::desktop::PowerControl::ACTION_SHUTDOWN, false);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Shutdown rejected by user";
        }
    }
    else if (action == ui.action_reboot)
    {
        LOG(LS_INFO) << "[ACTION] Reboot";
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you sure you want to reboot the remote computer?"),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        DesktopSettings settings;

        QCheckBox* wait_checkbox = new QCheckBox(&message_box);
        wait_checkbox->setText(tr("Wait for host"));
        wait_checkbox->setChecked(settings.waitForHost());
        message_box.setCheckBox(wait_checkbox);

        if (message_box.exec() == QMessageBox::Yes)
        {
            bool wait = wait_checkbox->isChecked();
            settings.setWaitForHost(wait);

            LOG(LS_INFO) << "[ACTION] Reboot accepted by user (wait=" << wait << ")";
            emit sig_powerControl(proto::desktop::PowerControl::ACTION_REBOOT, wait);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Reboot rejected by user";
        }
    }
    else if (action == ui.action_reboot_safe_mode)
    {
        LOG(LS_INFO) << "[ACTION] Reboot (safe mode)";
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you sure you want to reboot the remote computer in Safe Mode?"),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        DesktopSettings settings;

        QCheckBox* wait_checkbox = new QCheckBox(&message_box);
        wait_checkbox->setText(tr("Wait for host"));
        wait_checkbox->setChecked(settings.waitForHost());
        message_box.setCheckBox(wait_checkbox);

        if (message_box.exec() == QMessageBox::Yes)
        {
            bool wait = wait_checkbox->isChecked();
            settings.setWaitForHost(wait);

            LOG(LS_INFO) << "[ACTION] Reboot (safe mode) accepted by user (wait=" << wait << ")";
            emit sig_powerControl(proto::desktop::PowerControl::ACTION_REBOOT_SAFE_MODE, wait);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Reboot (safe mode) rejected by user";
        }
    }
    else if (action == ui.action_logoff)
    {
        LOG(LS_INFO) << "[ACTION] Logoff";
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you sure you want to end the user session on the remote computer?"),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Logoff accepted by user";
            emit sig_powerControl(proto::desktop::PowerControl::ACTION_LOGOFF, false);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Logoff rejected by user";
        }
    }
    else if (action == ui.action_lock)
    {
        LOG(LS_INFO) << "[ACTION] Lock";
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you sure you want to lock the user session on the remote computer?"),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Lock accepted by user";
            emit sig_powerControl(proto::desktop::PowerControl::ACTION_LOCK, false);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Lock rejected by user";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onChangeResolutionAction(QAction* action)
{
    QSize resolution = action->data().toSize();

    if (resolution == current_resolution_)
    {
        LOG(LS_INFO) << "Resolution not changed: "
                     << resolution.width() << "x" << resolution.height();
        return;
    }

    LOG(LS_INFO) << "[ACTION] Resolution selected: "
                 << resolution.width() << "x" << resolution.height();

    proto::desktop::Screen screen;
    screen.set_id(current_screen_id_);
    screen.mutable_resolution()->set_width(resolution.width());
    screen.mutable_resolution()->set_height(resolution.height());

    emit sig_screenSelected(screen);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onChangeScreenAction(QAction* action)
{
    const proto::desktop::Screen& screen = static_cast<SelectScreenAction*>(action)->screen();

    LOG(LS_INFO) << "[ACTION] Screen selected (id=" << screen.id() << " title="
                 << screen.title() << ")";

    proto::desktop::Screen out_screen;
    out_screen.set_id(screen.id());
    out_screen.set_title(screen.title());

    emit sig_screenSelected(screen);
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onMenuShow()
{
    allow_hide_ = false;
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onMenuHide()
{
    allow_hide_ = true;

    QTimer::singleShot(std::chrono::milliseconds(500), this, [this]()
    {
        if (!allow_hide_)
            return;

        if (!rect().contains(mapFromGlobal(QCursor::pos())))
            leaved_ = true;

        if (leaved_)
            delayedHide();
    });
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::onShowRecordSettings()
{
    LOG(LS_INFO) << "[ACTION] Record settings";
    RecordSettingsDialog(this).exec();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::createAdditionalMenu(proto::SessionType session_type)
{
    LOG(LS_INFO) << "Create additional menu";

    // Create a menu and add actions to it.
    additional_menu_ = new QMenu(this);

    additional_menu_->addAction(ui.action_paste_clipboard_as_keystrokes);
    additional_menu_->addSeparator();

    scale_group_ = new QActionGroup(additional_menu_);
    scale_group_->addAction(ui.action_scale100);
    scale_group_->addAction(ui.action_scale90);
    scale_group_->addAction(ui.action_scale80);
    scale_group_->addAction(ui.action_scale70);
    scale_group_->addAction(ui.action_scale60);
    scale_group_->addAction(ui.action_scale50);

    scale_menu_ = additional_menu_->addMenu(tr("Scale"));
    scale_menu_->addAction(ui.action_fit_window);
    scale_menu_->addSeparator();
    scale_menu_->addActions(scale_group_->actions());

    updateScaleMenu();

    additional_menu_->addAction(ui.action_autoscroll);

    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
        additional_menu_->addAction(ui.action_send_key_combinations);

    additional_menu_->addAction(ui.action_pause_video);
    additional_menu_->addAction(ui.action_pause_audio);

    additional_menu_->addSeparator();
    additional_menu_->addAction(ui.action_screenshot);
    additional_menu_->addSeparator();
    additional_menu_->addAction(ui.action_recording_settings);
    additional_menu_->addAction(ui.action_start_recording);
    additional_menu_->addSeparator();
    additional_menu_->addAction(ui.action_statistics);

    // Set the menu for the button on the toolbar.
    ui.action_menu->setMenu(additional_menu_);

    QToolButton* button = qobject_cast<QToolButton*>(ui.toolbar->widgetForAction(ui.action_menu));
    button->setPopupMode(QToolButton::InstantPopup);

    // Now we connect all the necessary signals and slots.
    if (session_type == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        connect(ui.action_send_key_combinations, &QAction::triggered,
                this, [this](bool enable)
        {
            LOG(LS_INFO) << "[ACTION] Send key combinations changed: " << enable;
            emit sig_keyCombinationsChanged(enable);
        });
    }

    connect(ui.action_paste_clipboard_as_keystrokes, &QAction::triggered,
            this, &DesktopToolBar::sig_pasteAsKeystrokes);

    connect(scale_group_, &QActionGroup::triggered, this, [this](QAction* action)
    {
        if (action == ui.action_scale100)
            scale_ = 100;
        else if (action == ui.action_scale90)
            scale_ = 90;
        else if (action == ui.action_scale80)
            scale_ = 80;
        else if (action == ui.action_scale70)
            scale_ = 70;
        else if (action == ui.action_scale60)
            scale_ = 60;
        else if (action == ui.action_scale50)
            scale_ = 50;
        else
            return;

        LOG(LS_INFO) << "[ACTION] Scale chenged: " << scale_;

        emit sig_scaleChanged();
    });

    connect(ui.action_fit_window, &QAction::toggled, this, [this](bool checked)
    {
        ui.action_autoscroll->setEnabled(!checked);
        scale_group_->setEnabled(!checked);

        if (checked)
        {
            scale_ = -1;
        }
        else
        {
            if (ui.action_scale90->isChecked())
                scale_ = 90;
            else if (ui.action_scale80->isChecked())
                scale_ = 80;
            else if (ui.action_scale70->isChecked())
                scale_ = 70;
            else if (ui.action_scale60->isChecked())
                scale_ = 60;
            else if (ui.action_scale50->isChecked())
                scale_ = 50;
            else
                scale_ = 100;
        }

        LOG(LS_INFO) << "[ACTION] Fit window changed (checked=" << checked << " scale=" << scale_ << ")";

        emit sig_scaleChanged();
    });

    connect(ui.action_pause_video, &QAction::triggered, this, [this](bool enable)
    {
        LOG(LS_INFO) << "[ACTION] Video pause changed: " << enable;
        emit sig_videoPauseChanged(enable);
    });
    connect(ui.action_pause_audio, &QAction::triggered, this, [this](bool enable)
    {
        LOG(LS_INFO) << "[ACTION] Audio pause changed: " << enable;
        emit sig_audioPauseChanged(enable);
    });
    connect(ui.action_screenshot, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Take screenshot";
        emit sig_takeScreenshot();
    });
    connect(additional_menu_, &QMenu::aboutToShow, this, &DesktopToolBar::onMenuShow);
    connect(additional_menu_, &QMenu::aboutToHide, this, &DesktopToolBar::onMenuHide);

    connect(ui.action_recording_settings, &QAction::triggered, this, &DesktopToolBar::onShowRecordSettings);
    connect(ui.action_start_recording, &QAction::triggered, this, [this]()
    {
        startRecording(!is_recording_started_);
    });
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::showFullScreenButtons(bool show)
{
    LOG(LS_INFO) << "Show full screen buttons: " << show;

    // MacOS does not have the ability to minimize a window from full screen mode. Therefore, for
    // MacOS we disable the minimize button from full screen mode.
    // For more info see: https://bugreports.qt.io/browse/QTBUG-62991
#if defined(Q_OS_MACOS)
    ui.action_minimize->setVisible(false);
    ui.action_minimize->setEnabled(false);
#else
    ui.action_minimize->setVisible(show);
    ui.action_minimize->setEnabled(show);
#endif

    ui.action_close->setVisible(show);
    ui.action_close->setEnabled(show);

    QList<QAction*> actions = ui.toolbar->actions();

    for (auto it = actions.crbegin(); it != actions.crend(); ++it)
    {
        QAction* action = *it;

        if (action->isSeparator())
        {
            action->setVisible(show);
            break;
        }
    }

    updateSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::updateScaleMenu()
{
    if (scale_ == -1)
    {
        ui.action_autoscroll->setEnabled(false);
        ui.action_fit_window->setChecked(true);
        scale_group_->setEnabled(false);
    }
    else
    {
        ui.action_autoscroll->setEnabled(true);
        ui.action_fit_window->setChecked(false);
        scale_group_->setEnabled(true);

        if (scale_ == 90)
            ui.action_scale90->setChecked(true);
        else if (scale_ == 80)
            ui.action_scale80->setChecked(true);
        else if (scale_ == 70)
            ui.action_scale70->setChecked(true);
        else if (scale_ == 60)
            ui.action_scale60->setChecked(true);
        else if (scale_ == 50)
            ui.action_scale50->setChecked(true);
        else
        {
            ui.action_scale100->setChecked(true);
            scale_ = 100;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::updateSize()
{
    ui.toolbar->adjustSize();
    adjustSize();
}

//--------------------------------------------------------------------------------------------------
void DesktopToolBar::delayedHide()
{
    if (!ui.action_pin->isChecked() && !hide_timer_->isActive())
        hide_timer_->start(std::chrono::seconds(1));
}

} // namespace client
