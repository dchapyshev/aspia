//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_config_dialog.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
#define _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H

#include "protocol/computer.pb.h"
#include "ui_desktop_config_dialog.h"

namespace aspia {

class DesktopConfigDialog : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(proto::auth::SessionType session_type,
                        proto::desktop::Config* config,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog();

private slots:
    void OnCodecChanged(int item_index);
    void OnCompressionRatioChanged(int value);
    void OnButtonBoxClicked(QAbstractButton* button);

private:
    Ui::DesktopConfigDialog ui;
    proto::auth::SessionType session_type_;
    proto::desktop::Config* config_;

    Q_DISABLE_COPY(DesktopConfigDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
