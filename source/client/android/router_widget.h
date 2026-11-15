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

#ifndef CLIENT_ANDROID_ROUTER_WIDGET_H
#define CLIENT_ANDROID_ROUTER_WIDGET_H

#include <QList>
#include <QWidget>

#include "client/android/router_card.h"

class IconButton;
class RouterEditor;
class RouterEmptyView;
class ScrollArea;
class QStackedWidget;
class QVBoxLayout;

class RouterWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RouterWidget(QWidget* parent = nullptr);
    ~RouterWidget() final;

    QList<QWidget*> appBarActions() const;

    void reload();

    // Returns to the list from the editor. Driven by the app bar back button.
    void goBack();

    // Returns to the list page without saving, used when the tab is left.
    void resetToList();

signals:
    // Requests the host bar to show |title| with a back button (editor) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

    // Emitted when the set returned by appBarActions() changes.
    void sig_appBarActionsChanged();

    // The button of the card asks the two-factor question again.
    void sig_twoFactorClicked();

private slots:
    void onAddRouter();
    void onClearEvents();
    void onEditRouter();
    void onRouterEvent(const RouterEvent& event);
    void returnFromEditor();

private:
    void showList();
    bool isEditorPage() const;
    void removeCard();

    QStackedWidget* stack_ = nullptr;
    ScrollArea* scroll_ = nullptr;
    QWidget* container_ = nullptr;
    QVBoxLayout* cards_layout_ = nullptr;
    RouterEmptyView* placeholder_ = nullptr;
    RouterEditor* editor_ = nullptr;
    IconButton* button_add_ = nullptr;
    IconButton* button_clear_ = nullptr;
    IconButton* button_edit_ = nullptr;
    RouterCard* card_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

#endif // CLIENT_ANDROID_ROUTER_WIDGET_H
