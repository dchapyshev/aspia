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

#include "console/import_export_util.h"

#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

#include "base/logging.h"
#include "base/desktop/pixel_format.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "console/computer_factory.h"

namespace console {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;
constexpr int kMinCompressRatio = 1;
constexpr int kMaxCompressRatio = 22;
constexpr int kDefaultCompressRatio = 8;

//--------------------------------------------------------------------------------------------------
bool isValidVideoEncoding(int video_encoding)
{
    switch (static_cast<proto::desktop::VideoEncoding>(video_encoding))
    {
        case proto::desktop::VIDEO_ENCODING_ZSTD:
        case proto::desktop::VIDEO_ENCODING_VP8:
        case proto::desktop::VIDEO_ENCODING_VP9:
            return true;

        default:
            return false;
    }
}

//--------------------------------------------------------------------------------------------------
bool isValidAudioEncoding(int audio_encoding)
{
    switch (static_cast<proto::desktop::AudioEncoding>(audio_encoding))
    {
        case proto::desktop::AUDIO_ENCODING_UNKNOWN:
        case proto::desktop::AUDIO_ENCODING_DEFAULT:
        case proto::desktop::AUDIO_ENCODING_OPUS:
            return true;

        default:
            return false;
    }
}

//--------------------------------------------------------------------------------------------------
bool isHostId(const QString& str)
{
    bool host_id_entered = true;

    for (int i = 0; i < str.length(); ++i)
    {
        if (!str[i].isDigit())
        {
            host_id_entered = false;
            break;
        }
    }

    return host_id_entered;
}

//--------------------------------------------------------------------------------------------------
int readHost(const QJsonObject& host, proto::address_book::ComputerGroup* group)
{
    QString computer_name = host["computer_name"].toString();
    if (computer_name.size() < kMinNameLength || computer_name.size() > kMaxNameLength)
    {
        LOG(LS_ERROR) << "Invalid computer name length:" << computer_name.size();
        return 0;
    }

    QString ip_address = host["ip_address"].toString();
    QString operating_system = host["operating_system"].toString();
    QString comment = QString("IP: '%1'\nOS: '%2'").arg(ip_address, operating_system);

    QJsonArray host_ids = host["host_ids"].toArray();
    if (host_ids.isEmpty())
    {
        LOG(LS_ERROR) << "Empty 'host_ids'";

        proto::address_book::Computer* computer = group->add_computer();
        computer->set_name(computer_name.toStdString());
        computer->set_comment(comment.toStdString());
        computer->set_address(std::string()); // Empty address
        computer->set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
        computer->set_port(DEFAULT_HOST_TCP_PORT);

        proto::address_book::InheritConfig* inherit = computer->mutable_inherit();
        inherit->set_credentials(true);
        inherit->set_desktop_manage(true);
        inherit->set_desktop_view(true);

        ComputerFactory::setDefaultDesktopManageConfig(
            computer->mutable_session_config()->mutable_desktop_manage());
        ComputerFactory::setDefaultDesktopViewConfig(
            computer->mutable_session_config()->mutable_desktop_view());

        qint64 current_time = QDateTime::currentSecsSinceEpoch();
        computer->set_create_time(current_time);
        computer->set_modify_time(current_time);

        return 1;
    }

    int count = 0;

    for (int i = 0; i < host_ids.size(); ++i)
    {
        QString host_id = host_ids[i].toString();
        if (!isHostId(host_id))
        {
            LOG(LS_INFO) << "Invalid host id:" << host_id;
            continue;
        }

        proto::address_book::Computer* computer = group->add_computer();
        computer->set_name(computer_name.toStdString());
        computer->set_comment(comment.toStdString());
        computer->set_address(host_id.toStdString());
        computer->set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
        computer->set_port(DEFAULT_HOST_TCP_PORT);

        proto::address_book::InheritConfig* inherit = computer->mutable_inherit();
        inherit->set_credentials(true);
        inherit->set_desktop_manage(true);
        inherit->set_desktop_view(true);

        ComputerFactory::setDefaultDesktopManageConfig(
            computer->mutable_session_config()->mutable_desktop_manage());
        ComputerFactory::setDefaultDesktopViewConfig(
            computer->mutable_session_config()->mutable_desktop_view());

        qint64 current_time = QDateTime::currentSecsSinceEpoch();
        computer->set_create_time(current_time);
        computer->set_modify_time(current_time);

        ++count;
    }

    return count;
}

//--------------------------------------------------------------------------------------------------
proto::address_book::InheritConfig readInheritConfig(
    const QJsonObject& json_inherit_config)
{
    proto::address_book::InheritConfig inherit_config;
    inherit_config.set_credentials(json_inherit_config["credentials"].toBool(true));
    inherit_config.set_desktop_manage(json_inherit_config["desktop_manage"].toBool(true));
    inherit_config.set_desktop_view(json_inherit_config["desktop_view"].toBool(true));
    return inherit_config;
}

//--------------------------------------------------------------------------------------------------
proto::desktop::PixelFormat readPixelFormat(QJsonObject& json_pixel_format)
{
    base::PixelFormat pixel_format;
    pixel_format.setBitsPerPixel(json_pixel_format["bits_per_pixel"].toInt());

    pixel_format.setRedMax(json_pixel_format["red_max"].toInt());
    pixel_format.setGreenMax(json_pixel_format["green_max"].toInt());
    pixel_format.setBlueMax(json_pixel_format["blue_max"].toInt());

    pixel_format.setRedShift(json_pixel_format["red_shift"].toInt());
    pixel_format.setGreenShift(json_pixel_format["green_shift"].toInt());
    pixel_format.setBlueShift(json_pixel_format["blue_shift"].toInt());

    proto::desktop::PixelFormat proto_pixel_format;

    if (pixel_format != base::PixelFormat::ARGB() && pixel_format != base::PixelFormat::RGB565() &&
        pixel_format != base::PixelFormat::RGB332() && pixel_format != base::PixelFormat::RGB222() &&
        pixel_format != base::PixelFormat::RGB111())
    {
        LOG(LS_ERROR) << "Invalid pixel format:" << pixel_format;
        pixel_format = base::PixelFormat::RGB332();
    }

    proto_pixel_format.set_bits_per_pixel(pixel_format.bitsPerPixel());

    proto_pixel_format.set_red_max(pixel_format.redMax());
    proto_pixel_format.set_green_max(pixel_format.greenMax());
    proto_pixel_format.set_blue_max(pixel_format.blueMax());

    proto_pixel_format.set_red_shift(pixel_format.redShift());
    proto_pixel_format.set_green_shift(pixel_format.greenShift());
    proto_pixel_format.set_blue_shift(pixel_format.blueShift());

    return proto_pixel_format;
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Config readDesktopConfig(const QJsonObject& json_desktop_config)
{
    int flags = json_desktop_config["flags"].toInt();

    int video_encoding = json_desktop_config["video_encoding"].toInt();
    if (!isValidVideoEncoding(video_encoding))
    {
        LOG(LS_ERROR) << "Invalid video encoding:" << video_encoding;
        video_encoding = static_cast<int>(proto::desktop::VIDEO_ENCODING_VP8);
    }

    int compress_ratio = json_desktop_config["compress_ratio"].toInt();
    if (compress_ratio < kMinCompressRatio || compress_ratio > kMaxCompressRatio)
    {
        LOG(LS_ERROR) << "Invalid compress ratio:" << compress_ratio;
        compress_ratio = kDefaultCompressRatio;
    }

    int audio_encoding = json_desktop_config["audio_encoding"].toInt();
    if (!isValidAudioEncoding(audio_encoding))
    {
        LOG(LS_ERROR) << "Invalid audio encoding:" << audio_encoding;
        audio_encoding = static_cast<int>(proto::desktop::AUDIO_ENCODING_OPUS);
    }

    QJsonObject json_pixel_format = json_desktop_config["pixel_format"].toObject();
    proto::desktop::PixelFormat pixel_format = readPixelFormat(json_pixel_format);

    proto::desktop::Config desktop_config;
    desktop_config.set_flags(flags);
    desktop_config.set_video_encoding(static_cast<proto::desktop::VideoEncoding>(video_encoding));
    desktop_config.set_compress_ratio(compress_ratio);
    desktop_config.set_audio_encoding(static_cast<proto::desktop::AudioEncoding>(audio_encoding));
    desktop_config.mutable_pixel_format()->CopyFrom(pixel_format);

    return desktop_config;
}

//--------------------------------------------------------------------------------------------------
proto::address_book::SessionConfig readSessionConfig(const QJsonObject& json_session_config)
{
    QJsonObject json_desktop_manage = json_session_config["desktop_manage"].toObject();
    proto::desktop::Config desktop_manage = readDesktopConfig(json_desktop_manage);

    QJsonObject json_desktop_view = json_session_config["desktop_view"].toObject();
    proto::desktop::Config desktop_view = readDesktopConfig(json_desktop_view);

    proto::address_book::SessionConfig session_config;
    session_config.mutable_desktop_manage()->CopyFrom(desktop_manage);
    session_config.mutable_desktop_view()->CopyFrom(desktop_view);

    return session_config;
}

//--------------------------------------------------------------------------------------------------
proto::address_book::ComputerGroupConfig readComputerGroupConfig(
    const QJsonObject& json_computer_group_config)
{
    QJsonObject json_session_config = json_computer_group_config["session_config"].toObject();
    proto::address_book::SessionConfig session_config = readSessionConfig(json_session_config);

    QJsonObject json_inherit = json_computer_group_config["inherit"].toObject();
    proto::address_book::InheritConfig inherit = readInheritConfig(json_inherit);

    QString username = json_computer_group_config["username"].toString();
    QString password = json_computer_group_config["password"].toString();

    if (!username.isEmpty() && !base::User::isValidUserName(username))
    {
        LOG(LS_ERROR) << "Invalid user name:" << username.toStdString();
        username.clear();
    }

    proto::address_book::ComputerGroupConfig computer_group_config;
    computer_group_config.mutable_session_config()->CopyFrom(session_config);
    computer_group_config.mutable_inherit()->CopyFrom(inherit);
    computer_group_config.set_username(username.toStdString());
    computer_group_config.set_password(password.toStdString());

    return computer_group_config;
}

//--------------------------------------------------------------------------------------------------
void readComputer(const QJsonObject& json_computer, proto::address_book::ComputerGroup* parent_group)
{
    QString name = json_computer["name"].toString();
    if (name.size() < kMinNameLength || name.size() > kMaxNameLength)
    {
        LOG(LS_ERROR) << "Invalid 'name' length:" << name.size();
        return;
    }

    QString comment = json_computer["comment"].toString();
    if (comment.size() > kMaxCommentLength)
    {
        LOG(LS_ERROR) << "Too long 'comment' length:" << comment.size();
        return;
    }

    QString address = json_computer["address"].toString();
    int port = json_computer["port"].toInt();

    if (!address.isEmpty() && !isHostId(address))
    {
        base::Address addr(DEFAULT_HOST_TCP_PORT);
        addr.setHost(address);
        addr.setPort(port);

        if (!addr.isValid())
        {
            LOG(LS_ERROR) << "Invalid 'address':" << address.toStdString();
            return;
        }
    }

    QString username = json_computer["username"].toString();
    if (!username.isEmpty() && !base::User::isValidUserName(username))
    {
        LOG(LS_ERROR) << "Invalid 'username':" << username.toStdString();
        return;
    }

    QString password = json_computer["password"].toString();

    QJsonObject json_session_config = json_computer["session_config"].toObject();
    proto::address_book::SessionConfig session_config = readSessionConfig(json_session_config);

    QJsonObject json_inherit = json_computer["inherit"].toObject();
    proto::address_book::InheritConfig inherit = readInheritConfig(json_inherit);

    qint64 current_time = QDateTime::currentSecsSinceEpoch();

    proto::address_book::Computer* computer = parent_group->add_computer();
    computer->set_create_time(current_time);
    computer->set_modify_time(current_time);
    computer->set_name(name.toStdString());
    computer->set_comment(comment.toStdString());
    computer->set_address(address.toStdString());
    computer->set_port(port);
    computer->set_username(username.toStdString());
    computer->set_password(password.toStdString());
    computer->set_session_type(proto::peer::SESSION_TYPE_DESKTOP_MANAGE);
    computer->mutable_session_config()->CopyFrom(session_config);
    computer->mutable_inherit()->CopyFrom(inherit);
}

//--------------------------------------------------------------------------------------------------
int readComputerGroup(
    const QJsonObject& json_computer_group, proto::address_book::ComputerGroup* parent_group)
{
    QString name = json_computer_group["name"].toString();
    if (name.size() < kMinNameLength || name.size() > kMaxNameLength)
    {
        LOG(LS_ERROR) << "Invalid 'name' length:" << name.size();
        return 0;
    }

    QString comment = json_computer_group["comment"].toString();
    if (comment.size() > kMaxCommentLength)
    {
        LOG(LS_ERROR) << "Too long 'comment' length:" << comment.size();
        return 0;
    }

    QJsonObject json_config = json_computer_group["config"].toObject();
    proto::address_book::ComputerGroupConfig computer_group_config =
        readComputerGroupConfig(json_config);

    proto::address_book::ComputerGroup* computer_group = parent_group->add_computer_group();
    computer_group->set_name(name.toStdString());
    computer_group->set_comment(comment.toStdString());
    computer_group->mutable_config()->CopyFrom(computer_group_config);
    computer_group->set_expanded(false);

    int count = 0;

    QJsonArray json_computers = json_computer_group["computer"].toArray();
    for (int i = 0; i < json_computers.size(); ++i)
    {
        readComputer(json_computers[i].toObject(), computer_group);
        ++count;
    }

    QJsonArray json_computer_groups = json_computer_group["computer_group"].toArray();
    for (int i = 0; i < json_computer_groups.size(); ++i)
    {
        count += readComputerGroup(json_computer_groups[i].toObject(), computer_group);
    }

    return count;
}

QJsonObject writePixelFormat(const proto::desktop::PixelFormat& pixel_format)
{
    QJsonObject json_pixel_format;

    json_pixel_format.insert("bits_per_pixel", static_cast<int>(pixel_format.bits_per_pixel()));

    json_pixel_format.insert("red_max", static_cast<int>(pixel_format.red_max()));
    json_pixel_format.insert("green_max", static_cast<int>(pixel_format.green_max()));
    json_pixel_format.insert("blue_max", static_cast<int>(pixel_format.blue_max()));

    json_pixel_format.insert("red_shift", static_cast<int>(pixel_format.red_shift()));
    json_pixel_format.insert("green_shift", static_cast<int>(pixel_format.green_shift()));
    json_pixel_format.insert("blue_shift", static_cast<int>(pixel_format.blue_shift()));

    return json_pixel_format;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeDesktopConfig(const proto::desktop::Config& desktop_config)
{
    QJsonObject json_desktop_config;

    json_desktop_config.insert("flags", static_cast<int>(desktop_config.flags()));
    json_desktop_config.insert("video_encoding", static_cast<int>(desktop_config.video_encoding()));
    json_desktop_config.insert("pixel_format", writePixelFormat(desktop_config.pixel_format()));
    json_desktop_config.insert("compress_ratio", static_cast<int>(desktop_config.compress_ratio()));
    json_desktop_config.insert("audio_encoding", static_cast<int>(desktop_config.audio_encoding()));

    return json_desktop_config;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeSessionConfig(const proto::address_book::SessionConfig& session_config)
{
    QJsonObject json_session_config;

    json_session_config.insert("desktop_manage", writeDesktopConfig(session_config.desktop_manage()));
    json_session_config.insert("desktop_view", writeDesktopConfig(session_config.desktop_view()));

    return json_session_config;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeInheritConfig(const proto::address_book::InheritConfig& inherit)
{
    QJsonObject json_inherit;

    json_inherit.insert("credentials", inherit.credentials());
    json_inherit.insert("desktop_manage", inherit.desktop_manage());
    json_inherit.insert("desktop_view", inherit.desktop_view());

    return json_inherit;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeComputer(const proto::address_book::Computer& computer)
{
    QJsonObject json_computer;

    json_computer.insert("name", QString::fromStdString(computer.name()));
    json_computer.insert("comment", QString::fromStdString(computer.comment()));
    json_computer.insert("address", QString::fromStdString(computer.address()));
    json_computer.insert("port", static_cast<int>(computer.port()));
    json_computer.insert("username", QString::fromStdString(computer.username()));
    json_computer.insert("password", QString::fromStdString(computer.password()));
    json_computer.insert("inherit", writeInheritConfig(computer.inherit()));
    json_computer.insert("session_config", writeSessionConfig(computer.session_config()));

    return json_computer;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeComputerGroupConfig(
    const proto::address_book::ComputerGroupConfig& computer_group_config)
{
    QJsonObject json_computer_group_config;

    json_computer_group_config.insert(
        "inherit", writeInheritConfig(computer_group_config.inherit()));
    json_computer_group_config.insert(
        "username", QString::fromStdString(computer_group_config.username()));
    json_computer_group_config.insert(
        "password", QString::fromStdString(computer_group_config.password()));
    json_computer_group_config.insert(
        "session_config", writeSessionConfig(computer_group_config.session_config()));

    return json_computer_group_config;
}

//--------------------------------------------------------------------------------------------------
QJsonObject writeComputerGroup(const proto::address_book::ComputerGroup& computer_group, int* count)
{
    QJsonArray json_computers;
    for (int i = 0; i < computer_group.computer_size(); ++i)
    {
        QJsonObject json_computer = writeComputer(computer_group.computer(i));
        json_computers.append(json_computer);
        ++*count;
    }

    QJsonArray json_computer_groups;
    for (int i = 0; i < computer_group.computer_group_size(); ++i)
    {
        QJsonObject json_computer_group =
            writeComputerGroup(computer_group.computer_group(i), count);
        json_computer_groups.append(json_computer_group);
    }

    QJsonObject json_computer_group;

    json_computer_group.insert("computer", json_computers);
    json_computer_group.insert("computer_group", json_computer_groups);
    json_computer_group.insert("name", QString::fromStdString(computer_group.name()));
    json_computer_group.insert("comment", QString::fromStdString(computer_group.comment()));
    json_computer_group.insert("config", writeComputerGroupConfig(computer_group.config()));

    return json_computer_group;
}

} // namespace

//--------------------------------------------------------------------------------------------------
void importComputersFromJson(
    const QJsonDocument& json, proto::address_book::ComputerGroup* parent_computer_group)
{
    QJsonObject root_object = json.object();
    int count = 0;

    if (root_object.contains("hosts"))
    {
        LOG(LS_INFO) << "JSON contains 'hosts' array";

        QJsonArray hosts = json["hosts"].toArray();
        if (hosts.isEmpty())
        {
            LOG(LS_INFO) << "Empty 'hosts' array";
            return;
        }

        for (int i = 0; i < hosts.size(); ++i)
        {
            count += readHost(hosts[i].toObject(), parent_computer_group);
        }
    }
    else if (root_object.contains("computer_group"))
    {
        LOG(LS_INFO) << "JSON contains 'computer_group' object";

        QJsonObject json_computer_group = json["computer_group"].toObject();
        if (json_computer_group.isEmpty())
        {
            LOG(LS_INFO) << "Empty 'computer_group' object";
            return;
        }

        count = readComputerGroup(json_computer_group, parent_computer_group);
    }

    LOG(LS_INFO) << "Imported computers:" << count;
}

//--------------------------------------------------------------------------------------------------
QJsonDocument exportComputersToJson(const proto::address_book::ComputerGroup& computer_group)
{
    QJsonObject root_object;
    int count = 0;

    root_object.insert("computer_group", writeComputerGroup(computer_group, &count));
    LOG(LS_INFO) << "Exported computers:" << count;

    return QJsonDocument(root_object);
}

} // namespace console
