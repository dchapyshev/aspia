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

#ifndef CLIENT_ANDROID_LOCAL_WIDGET_H
#define CLIENT_ANDROID_LOCAL_WIDGET_H

#include <QList>
#include <QString>
#include <QWidget>

class IconButton;
class LocalGroupEditor;
class LocalHostEditor;
class OnlineChecker;
class TreeWidget;
class QStackedWidget;
class QTreeWidgetItem;

// Local screen for the Android client: the address book stored locally on the device. A tree of
// groups whose entries may connect either directly or through a router; a tap on a group opens its
// host list. Mirrors the remote screen but reads synchronously from the local database.
class LocalWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LocalWidget(QWidget* parent = nullptr);
    ~LocalWidget() final;

    // Action buttons hosted in the app bar while the local screen is active.
    QList<QWidget*> appBarActions() const;

    // Rebuilds the group tree (after the cryptor is unlocked or the address book changes).
    void reload();

    // Returns to the tree from the host list. Driven by the app bar back button.
    void goBack();

    void retranslate();

signals:
    // Requests the host bar to show |title| with a back button (host list) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

    // Emitted when the set returned by appBarActions() changes (the editor hides the actions).
    void sig_appBarActionsChanged();

    // Requests a desktop connection to the local address book entry with the given id.
    void sig_connectHost(qint64 entry_id);

private slots:
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onShowMenu();
    void onImport();
    void onExport();
    void onAddGroup();
    void onAddHost();
    void onOnlineCheckerResult(qint64 entry_id, bool online);

private:
    void populateGroups(qint64 parent_id, QTreeWidgetItem* parent);
    void showTree();
    void showHosts(qint64 group_id, const QString& title);
    void editGroup(qint64 group_id);
    void editHost(qint64 host_id);
    void returnFromEditor();
    bool isEditorPage() const;

    QStackedWidget* stack_ = nullptr;
    TreeWidget* tree_ = nullptr;
    TreeWidget* host_tree_ = nullptr;
    LocalGroupEditor* group_editor_ = nullptr;
    LocalHostEditor* host_editor_ = nullptr;
    IconButton* search_button_ = nullptr;
    IconButton* overflow_button_ = nullptr;
    OnlineChecker* online_checker_ = nullptr;

    // The group a new host is added to: the open group on the host page, or the root on the tree.
    qint64 current_group_id_ = 0;
    QString current_title_;

    Q_DISABLE_COPY_MOVE(LocalWidget)
};

#endif // CLIENT_ANDROID_LOCAL_WIDGET_H
