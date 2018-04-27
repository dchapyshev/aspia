//
// PROJECT:         Aspia
// FILE:            client/ui/client_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/client_dialog.h"

#include "client/ui/desktop_config_dialog.h"
#include "codec/video_util.h"

namespace aspia {

ClientDialog::ClientDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    setFixedSize(size());
    setDefaultConfig();

    ui.edit_address->setText(QString::fromStdString(computer_.address()));
    ui.spin_port->setValue(computer_.port());

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor-keyboard.png")),
                                   tr("Desktop Manage"),
                                   QVariant(proto::auth::SESSION_TYPE_DESKTOP_MANAGE));

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/monitor.png")),
                                   tr("Desktop View"),
                                   QVariant(proto::auth::SESSION_TYPE_DESKTOP_VIEW));

    ui.combo_session_type->addItem(QIcon(QStringLiteral(":/icon/folder-stand.png")),
                                   tr("File Transfer"),
                                   QVariant(proto::auth::SESSION_TYPE_FILE_TRANSFER));

    int current_session_type = ui.combo_session_type->findData(QVariant(computer_.session_type()));
    if (current_session_type != -1)
    {
        ui.combo_session_type->setCurrentIndex(current_session_type);
        sessionTypeChanged(current_session_type);
    }

    connect(ui.combo_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ClientDialog::sessionTypeChanged);

    connect(ui.button_session_config, &QPushButton::pressed,
            this, &ClientDialog::sessionConfigButtonPressed);

    connect(ui.button_connect, &QPushButton::pressed,
            this, &ClientDialog::connectButtonPressed);
}

ClientDialog::~ClientDialog() = default;

void ClientDialog::sessionTypeChanged(int item_index)
{
    proto::auth::SessionType session_type = static_cast<proto::auth::SessionType>(
        ui.combo_session_type->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            ui.button_session_config->setEnabled(true);
            break;

        default:
            ui.button_session_config->setEnabled(false);
            break;
    }
}

void ClientDialog::sessionConfigButtonPressed()
{
    proto::auth::SessionType session_type = static_cast<proto::auth::SessionType>(
        ui.combo_session_type->currentData().toInt());

    switch (session_type)
    {
        case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
            DesktopConfigDialog(session_type,
                                computer_.mutable_desktop_manage_session(),
                                this).exec();
            break;

        case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
            DesktopConfigDialog(session_type,
                                computer_.mutable_desktop_view_session(),
                                this).exec();
            break;

        default:
            break;
    }
}

void ClientDialog::connectButtonPressed()
{
    proto::auth::SessionType session_type = static_cast<proto::auth::SessionType>(
        ui.combo_session_type->currentData().toInt());

    computer_.set_address(ui.edit_address->text().toStdString());
    computer_.set_port(ui.spin_port->value());
    computer_.set_session_type(session_type);

    accept();
    close();
}

void ClientDialog::setDefaultConfig()
{
    computer_.set_port(kDefaultHostTcpPort);

    proto::desktop::Config* desktop_manage = computer_.mutable_desktop_manage_session();
    desktop_manage->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                              proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_manage->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_manage->set_update_interval(30);
    desktop_manage->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_manage->mutable_pixel_format());

    proto::desktop::Config* desktop_view = computer_.mutable_desktop_view_session();
    desktop_view->set_flags(0);
    desktop_view->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_view->set_update_interval(30);
    desktop_view->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_view->mutable_pixel_format());

    computer_.set_session_type(proto::auth::SESSION_TYPE_DESKTOP_MANAGE);
}

} // namespace aspia
