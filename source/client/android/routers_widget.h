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

#ifndef CLIENT_ANDROID_ROUTERS_WIDGET_H
#define CLIENT_ANDROID_ROUTERS_WIDGET_H

#include <QHash>
#include <QList>
#include <QWidget>

#include "client/android/router_card.h"

class IconButton;
class Router;
class RouterConfig;
class RoutersEmptyView;
class ScrollArea;
class QVBoxLayout;

// Routers screen for the Android client: the list of configured routers. Each router is a card whose
// connection status panel slides out below it on tap. The add and edit actions are exposed via
// appBarActions() to be hosted in the top app bar.
class RoutersWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RoutersWidget(QWidget* parent = nullptr);
    ~RoutersWidget() final;

    QList<QWidget*> appBarActions() const;

    void reload();
    void resetEditMode();
    void retranslate();

signals:
    // Emitted when the set returned by appBarActions() changes (the edit action is hidden while the
    // list is empty).
    void appBarActionsChanged();

private slots:
    void onAddRouter();
    void onToggleEditMode();
    void onCardExpandRequested(qint64 router_id);
    void onEditRouter(qint64 router_id);
    void onRemoveRouter(qint64 router_id);

private:
    void clearCards();

    // Connects to the configured routers, reusing existing sessions and dropping removed ones.
    void syncSessions();
    void createRouterSession(const RouterConfig& config);
    void requestTwoFactorCode(qint64 router_id, const QString& otpauth_uri);
    void addRouterEvent(qint64 router_id, RouterEvent::Severity severity, const QString& text);

    ScrollArea* scroll_ = nullptr;
    QWidget* container_ = nullptr;
    QVBoxLayout* cards_layout_ = nullptr;
    RoutersEmptyView* placeholder_ = nullptr;
    IconButton* add_button_ = nullptr;
    IconButton* edit_button_ = nullptr;
    bool edit_mode_ = false;
    qint64 expanded_router_id_ = -1;

    QHash<qint64, Router*> sessions_;
    QHash<qint64, QList<RouterEvent>> events_;
    QHash<qint64, RouterCard*> cards_;

    Q_DISABLE_COPY_MOVE(RoutersWidget)
};

#endif // CLIENT_ANDROID_ROUTERS_WIDGET_H
