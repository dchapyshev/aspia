//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_CONFIG_DIALOG_H
#define CLIENT_UI_DESKTOP_CONFIG_DIALOG_H

#include "base/macros_magic.h"
#include "proto/common.pb.h"
#include "proto/desktop.pb.h"

#include <QDialog>

class QAbstractButton;

namespace Ui {
class DesktopConfigDialog;
} // namespace Ui

namespace client {

class DesktopConfigDialog : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(proto::SessionType session_type,
                        const proto::DesktopConfig& config,
                        uint32_t video_encodings,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog() override;

    const proto::DesktopConfig& config() { return config_; }

signals:
    void configChanged(const proto::DesktopConfig& config);

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

#endif // CLIENT_UI_DESKTOP_CONFIG_DIALOG_H
