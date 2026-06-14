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

#ifndef CLIENT_ANDROID_ROUTER_CARD_H
#define CLIENT_ANDROID_ROUTER_CARD_H

#include <QDateTime>
#include <QList>
#include <QString>

#include "client/router.h"
#include "common/android/expandable_panel.h"

class IconButton;
class Label;
class QLabel;
class QVBoxLayout;

// A single router connection event shown in the status panel.
struct RouterEvent
{
    enum class Severity
    {
        INFO,
        WARNING,
        CRITICAL
    };

    QDateTime time;
    Severity severity = Severity::INFO;
    QString text;
};

// A router row in the routers list: an ExpandablePanel whose header shows the status icon and name
// (with edit and delete actions in edit mode) and whose panel lists the connection events.
class RouterCard final : public ExpandablePanel
{
    Q_OBJECT

public:
    RouterCard(qint64 router_id, const QString& name, QWidget* parent = nullptr);
    ~RouterCard() final;

    qint64 routerId() const { return router_id_; }

    void setName(const QString& name);
    void setStatus(Router::Status status);
    void setEditMode(bool edit_mode);

    void setEvents(const QList<RouterEvent>& events);
    void appendEvent(const RouterEvent& event);

signals:
    void expandRequested(qint64 router_id);
    void editRequested(qint64 router_id);
    void removeRequested(qint64 router_id);

private:
    void clearEvents();

    qint64 router_id_;
    QLabel* status_icon_ = nullptr;
    Label* name_label_ = nullptr;
    IconButton* edit_button_ = nullptr;
    IconButton* delete_button_ = nullptr;
    QVBoxLayout* events_layout_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterCard)
};

#endif // CLIENT_ANDROID_ROUTER_CARD_H
