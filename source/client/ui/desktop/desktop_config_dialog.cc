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

#include "client/ui/desktop/desktop_config_dialog.h"

#include <QPushButton>
#include <QTimer>

#include "base/logging.h"
#include "ui_desktop_config_dialog.h"

namespace client {

//--------------------------------------------------------------------------------------------------
DesktopConfigDialog::DesktopConfigDialog(proto::peer::SessionType session_type,
    const proto::control::Config& config, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::DesktopConfigDialog>()),
      config_(config)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    QPushButton* cancel_button = ui->button_box->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    ui->checkbox_audio->setChecked(config_.audio());

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        ui->checkbox_lock_at_disconnect->setChecked(config_.lock_at_disconnect());
        ui->checkbox_block_remote_input->setChecked(config_.block_input());
        ui->checkbox_cursor_shape->setChecked(config_.cursor_shape());
        ui->checkbox_clipboard->setChecked(config_.clipboard());
    }
    else
    {
        ui->groupbox_other->hide();
        ui->checkbox_cursor_shape->hide();
        ui->checkbox_clipboard->hide();
    }

    ui->checkbox_enable_cursor_pos->setChecked(config_.cursor_position());
    ui->checkbox_desktop_effects->setChecked(!config_.effects());
    ui->checkbox_desktop_wallpaper->setChecked(!config_.wallpaper());

    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &DesktopConfigDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, [this]()
    {
        adjustSize();
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
DesktopConfigDialog::~DesktopConfigDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableAudioFeature(bool enable)
{
    LOG(INFO) << "enableAudioFeature:" << enable;
    ui->checkbox_audio->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableClipboardFeature(bool enable)
{
    LOG(INFO) << "enableClipboardFeature:" << enable;
    ui->checkbox_clipboard->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorShapeFeature(bool enable)
{
    LOG(INFO) << "enableCursorShapeFeature:" << enable;
    ui->checkbox_cursor_shape->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableCursorPositionFeature(bool enable)
{
    LOG(INFO) << "enableCursorPositionFeature:" << enable;
    ui->checkbox_enable_cursor_pos->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopEffectsFeature(bool enable)
{
    LOG(INFO) << "enableDesktopEffectsFeature:" << enable;
    ui->checkbox_desktop_effects->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableDesktopWallpaperFeature(bool enable)
{
    LOG(INFO) << "enableDesktopWallpaperFeature:" << enable;
    ui->checkbox_desktop_wallpaper->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableLockAtDisconnectFeature(bool enable)
{
    LOG(INFO) << "enableLockAtDisconnectFeature:" << enable;
    ui->checkbox_lock_at_disconnect->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::enableBlockInputFeature(bool enable)
{
    LOG(INFO) << "enableBlockInputFeature:" << enable;
    ui->checkbox_block_remote_input->setEnabled(enable);
}

//--------------------------------------------------------------------------------------------------
void DesktopConfigDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        config_.set_audio(ui->checkbox_audio->isChecked());
        config_.set_cursor_shape(ui->checkbox_cursor_shape->isChecked() && ui->checkbox_cursor_shape->isEnabled());
        config_.set_cursor_position(ui->checkbox_enable_cursor_pos->isChecked());
        config_.set_clipboard(ui->checkbox_clipboard->isChecked() && ui->checkbox_clipboard->isEnabled());
        config_.set_effects(!ui->checkbox_desktop_effects->isChecked());
        config_.set_wallpaper(!ui->checkbox_desktop_wallpaper->isChecked());
        config_.set_block_input(ui->checkbox_block_remote_input->isChecked());
        config_.set_lock_at_disconnect(ui->checkbox_lock_at_disconnect->isChecked());

        emit sig_configChanged(config_);
        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
    }
}

} // namespace client
