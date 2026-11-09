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

#ifndef HOST_ANDROID_SETTINGS_WIDGET_H
#define HOST_ANDROID_SETTINGS_WIDGET_H

#include <QList>
#include <QWidget>

class AboutWidget;
class UpdateWidget;
class IconButton;
class ScrollArea;
class UserEditorWidget;
class UsersWidget;
class QStackedWidget;
class QVBoxLayout;

// Settings section of the Android host. Exposes the interface preferences (theme), applied immediately
// and stored in UserSettings. The about and user management screens are hosted as sub-pages reached from
// the top app bar action.
class SettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() final;

    // The app bar action (opens the about screen). Empty while the about screen is shown.
    QList<QWidget*> appBarActions() const;

    // Returns to the settings page from the about screen. Driven by the app bar back button.
    void goBack();

    // Returns to the settings page without animation, used when the tab is left.
    void resetToSettings();

signals:
    // Requests the app bar to show |title| with a back button (about) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

    // Emitted when the set returned by appBarActions() changes (the about screen hides the action).
    void sig_appBarActionsChanged();

    // Emitted after the router settings are stored.
    void sig_routerSettingsChanged();

    // Emitted after the update settings are stored.
    void sig_updateSettingsChanged();

    // Emitted after a user is created, modified or deleted.
    void sig_usersChanged();

    // Emitted after the password protection of the settings is enabled or disabled.
    void sig_passwordProtectionChanged();

private slots:
    void onShowAbout();
    void onShowUsers();
    void onShowUserEditor(qint64 entry_id);
    void onShowUpdate();
    void onEnableProtection();
    void onChangePassword();
    void onDisableProtection();
    void onImport();
    void onAddFolder();

private:
    bool isAboutPage() const;
    bool isUsersPage() const;
    bool isEditorPage() const;
    bool isUpdatePage() const;

    void buildSettings();
    void addSectionHeader(QVBoxLayout* layout, const QString& text);

    void buildInterfaceSection(QVBoxLayout* layout);
    void buildSecuritySection(QVBoxLayout* layout);
    void buildRouterSection(QVBoxLayout* layout);
    void buildFileTransferSection(QVBoxLayout* layout);
    void buildUpdateSection(QVBoxLayout* layout);

    void removeFolder(const QString& path, const QString& uri);

    QStackedWidget* stack_ = nullptr;
    ScrollArea* settings_page_ = nullptr;
    AboutWidget* about_page_ = nullptr;
    UpdateWidget* update_page_ = nullptr;
    UsersWidget* users_page_ = nullptr;
    UserEditorWidget* editor_page_ = nullptr;
    IconButton* about_button_ = nullptr;
    IconButton* import_button_ = nullptr;
    IconButton* add_user_button_ = nullptr;
    IconButton* save_button_ = nullptr;

    Q_DISABLE_COPY_MOVE(SettingsWidget)
};

#endif // HOST_ANDROID_SETTINGS_WIDGET_H
