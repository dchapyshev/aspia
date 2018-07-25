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

#ifndef ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H_
#define ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H_

#include "protocol/desktop_session.pb.h"
#include "ui_desktop_config_dialog.h"

namespace aspia {

class DesktopConfigDialog : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(const proto::desktop::Config& config,
                        uint32_t supported_video_encodings,
                        uint32_t supported_features,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog() = default;

    const proto::desktop::Config& config() { return config_; }

private slots:
    void onCodecChanged(int item_index);
    void onCompressionRatioChanged(int value);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::DesktopConfigDialog ui;

    proto::desktop::Config config_;
    uint32_t supported_video_encodings_;
    uint32_t supported_features_;

    Q_DISABLE_COPY(DesktopConfigDialog)
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H_
