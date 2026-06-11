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

#ifndef CLIENT_ANDROID_ROUTER_STATUS_DIALOG_H
#define CLIENT_ANDROID_ROUTER_STATUS_DIALOG_H

#include <QDateTime>
#include <QList>
#include <QString>

#include "common/android/dialog.h"

class QVBoxLayout;
class ScrollArea;

// A single router connection event shown in the status dialog.
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

// Read-only dialog listing the connection events of a router, appended to live while it is open.
class RouterStatusDialog final : public Dialog
{
    Q_OBJECT

public:
    RouterStatusDialog(const QString& title, qint64 router_id, const QList<RouterEvent>& events,
                       QWidget* parent = nullptr);
    ~RouterStatusDialog() final;

signals:
    // Emitted when the user clears the log, so the owner can drop the stored events too.
    void sig_clearEvents(qint64 router_id);

public slots:
    void onRouterEvent(qint64 router_id, const RouterEvent& event);

private slots:
    void onClear();

private:
    void appendEvent(const RouterEvent& event);
    void scrollToBottom();

    ScrollArea* scroll_ = nullptr;
    QVBoxLayout* events_layout_ = nullptr;
    qint64 router_id_ = -1;

    Q_DISABLE_COPY_MOVE(RouterStatusDialog)
};

#endif // CLIENT_ANDROID_ROUTER_STATUS_DIALOG_H
