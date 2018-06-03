//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_config_dialog.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
#define _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H

#include "protocol/authorization.pb.h"
#include "protocol/desktop_session.pb.h"
#include "ui_desktop_config_dialog.h"

namespace aspia {

class DesktopConfigDialog : public QDialog
{
    Q_OBJECT

public:
    DesktopConfigDialog(proto::desktop::Config* config,
                        quint32 supported_video_encodings,
                        quint32 supported_features,
                        QWidget* parent = nullptr);
    ~DesktopConfigDialog();

private slots:
    void onCodecChanged(int item_index);
    void onCompressionRatioChanged(int value);
    void onButtonBoxClicked(QAbstractButton* button);

private:
    Ui::DesktopConfigDialog ui;

    proto::desktop::Config* config_;
    quint32 supported_video_encodings_;
    quint32 supported_features_;

    Q_DISABLE_COPY(DesktopConfigDialog)
};

} // namespace aspia

#endif // _ASPIA_CLIENT__UI__DESKTOP_CONFIG_DIALOG_H
