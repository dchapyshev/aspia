//
// PROJECT:         Aspia
// FILE:            console/computer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/computer.h"

#include "codec/video_util.h"
#include "console/computer_group.h"

namespace aspia {

Computer::Computer(proto::Computer* computer, ComputerGroup* parent_group)
    : computer_(computer),
    parent_group_(parent_group)
{
    setIcon(0, QIcon(QStringLiteral(":/icon/computer.png")));
    setText(0, QString::fromStdString(computer->name()));
    setText(1, QString::fromStdString(computer->address()));
    setText(2, QString::number(computer->port()));
}

Computer::~Computer()
{
    if (!parent_group_)
        delete computer_;
}

// static
std::unique_ptr<Computer> Computer::Create()
{
    std::unique_ptr<proto::Computer> computer = std::make_unique<proto::Computer>();

    computer->set_port(kDefaultHostTcpPort);

    proto::desktop::Config* desktop_manage = computer->mutable_desktop_manage_session();
    desktop_manage->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                              proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_manage->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_manage->set_update_interval(30);
    desktop_manage->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_manage->mutable_pixel_format());

    proto::desktop::Config* desktop_view = computer->mutable_desktop_view_session();
    desktop_view->set_flags(0);
    desktop_view->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_view->set_update_interval(30);
    desktop_view->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_view->mutable_pixel_format());

    return std::unique_ptr<Computer>(new Computer(computer.release(), nullptr));
}

QString Computer::Name() const
{
    return text(0);
}

void Computer::SetName(const QString& name)
{
    computer_->set_name(name.toStdString());
    setText(0, name);
}

QString Computer::Comment() const
{
    return QString::fromStdString(computer_->comment());
}

void Computer::SetComment(const QString& comment)
{
    computer_->set_comment(comment.toStdString());
}

QString Computer::Address() const
{
    return QString::fromStdString(computer_->address());
}

void Computer::SetAddress(const QString& address)
{
    computer_->set_address(address.toStdString());
    setText(1, address);
}

int Computer::Port() const
{
    return computer_->port();
}

void Computer::SetPort(int port)
{
    computer_->set_port(port);
    setText(2, QString::number(port));
}

QString Computer::UserName() const
{
    return QString::fromStdString(computer_->username());
}

void Computer::SetUserName(const QString& username)
{
    computer_->set_username(username.toStdString());
}

QString Computer::Password() const
{
    return QString::fromStdString(computer_->password());
}

void Computer::SetPassword(const QString& password)
{
    computer_->set_password(password.toStdString());
}

proto::desktop::Config Computer::DesktopManageSessionConfig() const
{
    return computer_->desktop_manage_session();
}

void Computer::SetDesktopManageSessionConfig(const proto::desktop::Config& config)
{
    computer_->mutable_desktop_manage_session()->CopyFrom(config);
}

proto::desktop::Config Computer::DesktopViewSessionConfig() const
{
    return computer_->desktop_view_session();
}

void Computer::SetDesktopViewSessionConfig(const proto::desktop::Config& config)
{
    computer_->mutable_desktop_view_session()->CopyFrom(config);
}

ComputerGroup* Computer::ParentComputerGroup()
{
    return parent_group_;
}

} // namespace aspia
