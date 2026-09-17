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

#ifndef CLIENT_ANDROID_CREDENTIALS_WIDGET_H
#define CLIENT_ANDROID_CREDENTIALS_WIDGET_H

#include <QList>
#include <QWidget>

class CredentialEditor;
class IconButton;
class TreeWidget;
class QStackedWidget;
class QTreeWidgetItem;

class CredentialsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit CredentialsWidget(QWidget* parent = nullptr);
    ~CredentialsWidget() final;

    QList<QWidget*> appBarActions() const;

    // Reads the records from the database again.
    void reload();

    // Returns to the list from the editor. Driven by the app bar back button.
    void goBack();
    bool isEditorPage() const;

signals:
    // Requests the app bar to show |title|.
    void sig_titleChanged(const QString& title);

    // Emitted when the set returned by appBarActions() changes (the editor hides the actions).
    void sig_appBarActionsChanged();

private slots:
    void onAddCredential();
    void onItemClicked(QTreeWidgetItem* item);
    void onReturnFromEditor();

private:
    void showList();

    QStackedWidget* stack_ = nullptr;
    TreeWidget* tree_ = nullptr;
    CredentialEditor* editor_ = nullptr;
    IconButton* button_add_ = nullptr;

    Q_DISABLE_COPY_MOVE(CredentialsWidget)
};

#endif // CLIENT_ANDROID_CREDENTIALS_WIDGET_H
