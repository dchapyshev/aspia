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

#ifndef CLIENT_UI_DESKTOP_DESKTOP_CONFIG_DIALOG_H
#define CLIENT_UI_DESKTOP_DESKTOP_CONFIG_DIALOG_H

#include "base/macros_magic.h"
#include "proto/common.h"
#include "proto/desktop.h"

#include <QDialog>

class QAbstractButton;

namespace Ui {
class DesktopConfigDialog;
} // namespace Ui

namespace client {

class DesktopConfigDialog final : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(proto::SessionType session_type,
                        const proto::DesktopConfig& config,
                        uint32_t video_encodings,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog() final;

    void enableAudioFeature(bool enable);
    void enableClipboardFeature(bool enable);
    void enableCursorShapeFeature(bool enable);
    void enableCursorPositionFeature(bool enable);
    void enableDesktopEffectsFeature(bool enable);
    void enableDesktopWallpaperFeature(bool enable);
    void enableFontSmoothingFeature(bool enable);
    void enableClearClipboardFeature(bool enable);
    void enableLockAtDisconnectFeature(bool enable);
    void enableBlockInputFeature(bool enable);

    const proto::DesktopConfig& config() { return config_; }

signals:
    void sig_configChanged(const proto::DesktopConfig& config);

private slots:
    void onCodecChanged(int item_index);
    void onCompressionRatioChanged(int value);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::DesktopConfigDialog> ui;
    proto::DesktopConfig config_;

    DISALLOW_COPY_AND_ASSIGN(DesktopConfigDialog);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_DESKTOP_CONFIG_DIALOG_H
