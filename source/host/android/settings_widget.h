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
class IconButton;
class ScrollArea;
class UserEditorWidget;
class UsersWidget;
class QStackedWidget;
class QVBoxLayout;

// Settings section of the Android host. Exposes the interface preferences (language and theme),
// applied immediately and stored in UserSettings. The about and user management screens are hosted as
// sub-pages reached from the top app bar action.
class SettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWidget(QWidget* parent = nullptr);
    ~SettingsWidget() final;

    // Rebuilds the content with the current language. Called by the parent on a language change,
    // because only top-level widgets receive QEvent::LanguageChange.
    void retranslate();

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

private:
    void showAbout();
    void showUsers();
    void showUserEditor(qint64 entry_id);
    bool isAboutPage() const;
    bool isUsersPage() const;
    bool isEditorPage() const;

    void buildSettings();
    void addSectionHeader(QVBoxLayout* layout, const QString& text);
    void addSessionSwitch(QVBoxLayout* layout, const QString& text, quint32 session_flag,
                          quint32 current);

    void buildInterfaceSection(QVBoxLayout* layout);
    void buildAccessSection(QVBoxLayout* layout);
    void buildUsersSection(QVBoxLayout* layout);
    void buildRouterSection(QVBoxLayout* layout);

    QStackedWidget* stack_;
    ScrollArea* settings_page_;
    AboutWidget* about_page_;
    UsersWidget* users_page_;
    UserEditorWidget* editor_page_;
    IconButton* about_button_;
    IconButton* add_user_button_;
    IconButton* save_button_;

    Q_DISABLE_COPY_MOVE(SettingsWidget)
};

#endif // HOST_ANDROID_SETTINGS_WIDGET_H
