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

#ifndef CLIENT_TELEMETRY_MODEL_H
#define CLIENT_TELEMETRY_MODEL_H

#include <QAbstractItemModel>
#include <QList>

class QJsonObject;

class TelemetryModel final : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Column
    {
        NAME,
        VALUE
    };

    enum class Status
    {
        OK,
        EMPTY, // The host has not reported telemetry yet.
        INVALID, // Not a telemetry document.
        UNSUPPORTED_VERSION // A version this client does not know.
    };

    explicit TelemetryModel(QObject* parent = nullptr);
    ~TelemetryModel() final;

    // Replaces the content with the telemetry in |json|. Parameters the model does not know are
    // skipped. Unless the answer is OK, the model is left empty.
    Status setTelemetry(const QByteArray& json);

    // QAbstractItemModel implementation.
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const final;
    QModelIndex parent(const QModelIndex& child) const final;
    int rowCount(const QModelIndex& parent = QModelIndex()) const final;
    int columnCount(const QModelIndex& parent = QModelIndex()) const final;
    QVariant data(const QModelIndex& index, int role) const final;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const final;

private:
    struct Parameter
    {
        QString name;
        QString value;
    };

    struct Group
    {
        QString name;
        QList<Parameter> parameters;
    };

    static Status parse(const QByteArray& json, QList<Group>* groups);
    static void parseVersion1(const QJsonObject& telemetry, QList<Group>* groups);
    static void parseUpdateGroup(const QJsonObject& update, QList<Group>* groups);
    static QString updateChannelName(const QString& channel);
    static QString checkResultName(const QString& result);
    static QString timeToString(qint64 time);

    QList<Group> groups_;

    Q_DISABLE_COPY_MOVE(TelemetryModel)
};

#endif // CLIENT_TELEMETRY_MODEL_H
