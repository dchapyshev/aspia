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

#ifndef CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
#define CLIENT__UI__DESKTOP_CONFIG_DIALOG_H

#include <QDialog>

#include "base/macros_magic.h"
#include "proto/common.pb.h"
#include "proto/desktop_session.pb.h"

namespace Ui {
class DesktopConfigDialog;
} // namespace Ui

class QAbstractButton;

namespace client {

class DesktopConfigDialog : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(proto::SessionType session_type,
                        const proto::desktop::Config& config,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog();

    const proto::desktop::Config& config() { return config_; }

signals:
    void configChanged(const proto::desktop::Config& config);

private slots:
    void onCodecChanged(int item_index);
    void onCompressionRatioChanged(int value);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    QScopedPointer<Ui::DesktopConfigDialog> ui;

    proto::desktop::Config config_;

    DISALLOW_COPY_AND_ASSIGN(DesktopConfigDialog);
};

} // namespace client

#endif // CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
