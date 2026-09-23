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

#include "client/telemetry_model.h"

#include <QDateTime>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include "base/build_config.h"
#include "base/logging.h"

namespace {

// The internal id of a group row. A parameter row carries the row of its group plus one.
const quintptr kGroupId = 0;

// The host keeps no more events of a kind (service starts, failed logins) than this.
const int kMaxEventCount = 1000;

} // namespace

//--------------------------------------------------------------------------------------------------
TelemetryModel::TelemetryModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TelemetryModel::~TelemetryModel()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
TelemetryModel::Status TelemetryModel::setTelemetry(const QByteArray& json)
{
    QList<Group> groups;
    const Status status = parse(json, &groups);

    beginResetModel();
    groups_ = std::move(groups);
    endResetModel();

    return status;
}

//--------------------------------------------------------------------------------------------------
QModelIndex TelemetryModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (!parent.isValid())
        return createIndex(row, column, kGroupId);

    return createIndex(row, column, static_cast<quintptr>(parent.row()) + 1);
}

//--------------------------------------------------------------------------------------------------
QModelIndex TelemetryModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.internalId() == kGroupId)
        return QModelIndex();

    return createIndex(static_cast<int>(child.internalId() - 1), 0, kGroupId);
}

//--------------------------------------------------------------------------------------------------
int TelemetryModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return static_cast<int>(groups_.size());

    // Only the first column of a group row has children.
    if (parent.internalId() != kGroupId || parent.column() != 0)
        return 0;

    return static_cast<int>(groups_.at(parent.row()).parameters.size());
}

//--------------------------------------------------------------------------------------------------
int TelemetryModel::columnCount(const QModelIndex& /* parent */) const
{
    return 2;
}

//--------------------------------------------------------------------------------------------------
QVariant TelemetryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (index.internalId() == kGroupId)
    {
        if (column != Column::NAME)
            return QVariant();

        if (role == Qt::DisplayRole)
            return groups_.at(index.row()).name;

        if (role == Qt::FontRole)
        {
            QFont font;
            font.setBold(true);
            return font;
        }

        return QVariant();
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    const Group& group = groups_.at(static_cast<int>(index.internalId() - 1));
    const Parameter& parameter = group.parameters.at(index.row());

    return column == Column::NAME ? parameter.name : parameter.value;
}

//--------------------------------------------------------------------------------------------------
QVariant TelemetryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::NAME:
            return tr("Parameter");

        case Column::VALUE:
            return tr("Value");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
// static
TelemetryModel::Status TelemetryModel::parse(const QByteArray& json, QList<Group>* groups)
{
    if (json.isEmpty())
        return Status::EMPTY;

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        LOG(ERROR) << "Unable to parse telemetry:" << error.errorString();
        return Status::INVALID;
    }

    const QJsonObject telemetry = document.object();

    const QJsonValue version = telemetry.value("version");
    if (!version.isDouble())
    {
        LOG(ERROR) << "Telemetry has no version";
        return Status::INVALID;
    }

    switch (version.toInt())
    {
        case 1:
            parseVersion1(telemetry, groups);
            return Status::OK;

        default:
            LOG(ERROR) << "Unsupported telemetry version:" << version.toInt();
            return Status::UNSUPPORTED_VERSION;
    }
}

//--------------------------------------------------------------------------------------------------
// static
void TelemetryModel::parseVersion1(const QJsonObject& telemetry, QList<Group>* groups)
{
    const QJsonValue general = telemetry.value("general");
    if (general.isObject())
        parseGeneralGroup(general.toObject(), groups);

    const QJsonValue update = telemetry.value("update");
    if (update.isObject())
        parseUpdateGroup(update.toObject(), groups);

    const QJsonValue connects = telemetry.value("connects");
    if (connects.isObject())
        parseConnectsGroup(connects.toObject(), groups);

    const QJsonValue users = telemetry.value("users");
    if (users.isObject())
        parseUsersGroup(users.toObject(), groups);
}

//--------------------------------------------------------------------------------------------------
// static
void TelemetryModel::parseGeneralGroup(const QJsonObject& general, QList<Group>* groups)
{
    Group group;
    group.name = tr("General");

    const QJsonValue start_time = general.value("start_time");
    if (start_time.isDouble())
        group.parameters.append({ tr("Service start time"), timeToString(start_time.toInteger()) });

    const QJsonValue start_count = general.value("start_count");
    if (start_count.isDouble())
        group.parameters.append({ tr("Service starts in 7 days"), countToString(start_count.toInt()) });

    if (!group.parameters.isEmpty())
        groups->append(group);
}

//--------------------------------------------------------------------------------------------------
// static
void TelemetryModel::parseConnectsGroup(const QJsonObject& connects, QList<Group>* groups)
{
    Group group;
    group.name = tr("Connections");

    const QJsonValue last_connect_time = connects.value("last_connect_time");
    if (last_connect_time.isDouble())
    {
        group.parameters.append(
            { tr("Last incoming connection"), timeToString(last_connect_time.toInteger()) });
    }

    const QJsonValue failed_logins = connects.value("failed_logins");
    if (failed_logins.isDouble())
        group.parameters.append({ tr("Failed logins in 7 days"), countToString(failed_logins.toInt()) });

    const QJsonValue failed_logins_since_start = connects.value("failed_logins_since_start");
    if (failed_logins_since_start.isDouble())
    {
        group.parameters.append({ tr("Failed logins since service start"),
                                  countToString(failed_logins_since_start.toInt()) });
    }

    if (!group.parameters.isEmpty())
        groups->append(group);
}

//--------------------------------------------------------------------------------------------------
// static
void TelemetryModel::parseUpdateGroup(const QJsonObject& update, QList<Group>* groups)
{
    Group group;
    group.name = tr("Updates");

    const QJsonValue channel = update.value("channel");
    if (channel.isString())
        group.parameters.append({ tr("Update channel"), updateChannelName(channel.toString()) });

    const QJsonValue auto_update = update.value("auto_update");
    if (auto_update.isBool())
    {
        group.parameters.append(
            { tr("Automatic updates"), auto_update.toBool() ? tr("Enabled") : tr("Disabled") });
    }

    const QJsonValue check_frequency = update.value("check_frequency");
    if (check_frequency.isDouble())
    {
        group.parameters.append(
            { tr("Update check frequency"), checkFrequencyName(check_frequency.toInt()) });
    }

    const QJsonValue last_check_time = update.value("last_check_time");
    if (last_check_time.isDouble())
        group.parameters.append({ tr("Last update check"), timeToString(last_check_time.toInteger()) });

    const QJsonValue last_check_result = update.value("last_check_result");
    if (last_check_result.isString())
    {
        group.parameters.append(
            { tr("Last update check result"), checkResultName(last_check_result.toString()) });
    }

    if (!group.parameters.isEmpty())
        groups->append(group);
}

//--------------------------------------------------------------------------------------------------
// static
void TelemetryModel::parseUsersGroup(const QJsonObject& users, QList<Group>* groups)
{
    Group group;
    group.name = tr("Users");

    const QJsonValue total = users.value("total");
    if (total.isDouble())
        group.parameters.append({ tr("Total users"), QString::number(total.toInt()) });

    const QJsonValue enabled = users.value("enabled");
    if (enabled.isDouble())
        group.parameters.append({ tr("Enabled users"), QString::number(enabled.toInt()) });

    if (!group.parameters.isEmpty())
        groups->append(group);
}

//--------------------------------------------------------------------------------------------------
// static
QString TelemetryModel::updateChannelName(const QString& channel)
{
    if (channel == kStableUpdateChannel)
        return tr("Stable");
    else if (channel == kBetaUpdateChannel)
        return tr("Beta");
    else if (channel == kAlphaUpdateChannel)
        return tr("Alpha");

    return channel;
}

//--------------------------------------------------------------------------------------------------
// static
QString TelemetryModel::checkFrequencyName(int days)
{
    // The host settings offer these three. Any other number of days comes only from an imported
    // configuration.
    if (days == 1)
        return tr("Once a day");
    else if (days == 7)
        return tr("Once a week");
    else if (days == 30)
        return tr("Once a month");

    return tr("Every %n days", "", days);
}

//--------------------------------------------------------------------------------------------------
// static
QString TelemetryModel::checkResultName(const QString& result)
{
    if (result == "no_update")
        return tr("No updates");
    else if (result == "check_failed")
        return tr("Check failed");
    else if (result == "unsupported_package")
        return tr("Unsupported package");
    else if (result == "download_failed")
        return tr("Download failed");
    else if (result == "damaged_package")
        return tr("Damaged package");
    else if (result == "install_failed")
        return tr("Installation failed");
    else if (result == "install_started")
        return tr("Installation started");
    else if (result == "install_succeeded")
        return tr("Installation succeeded");

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QString TelemetryModel::countToString(int count)
{
    if (count >= kMaxEventCount)
        return tr("%1 or more").arg(kMaxEventCount);

    return QString::number(count);
}

//--------------------------------------------------------------------------------------------------
// static
QString TelemetryModel::timeToString(qint64 time)
{
    if (time <= 0)
        return tr("Never");

    return QLocale::system().toString(QDateTime::fromSecsSinceEpoch(time), QLocale::ShortFormat);
}
