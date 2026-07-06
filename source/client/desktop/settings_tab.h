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

#ifndef CLIENT_DESKTOP_SETTINGS_TAB_H
#define CLIENT_DESKTOP_SETTINGS_TAB_H

#include <memory>

#include "client/desktop/tab.h"

namespace Ui {
class SettingsTab;
} // namespace Ui

class QButtonGroup;

class SettingsTab final : public Tab
{
    Q_OBJECT

public:
    explicit SettingsTab(QWidget* parent = nullptr);
    ~SettingsTab() final;

    // Tab implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;
    bool hasStatusBar() const final;

signals:
    void sig_desktopConfigChanged();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onCategoryChanged(int id);
    void onLanguageChanged();
    void onThemeChanged();
    void onDisplayNameChanged();
    void onUdpMethodsChanged();
    void onDesktopFeatureChanged();
    void onRecordAutostartChanged();
    void onRecordingPathChanged();
    void onSelectRecordingPath();
    void onChangeMasterPassword();
    void onCheckUpdatesChanged();
    void onCustomServerToggled(bool checked);
    void onUpdateServerChanged();
    void onCheckForUpdatesClicked();

private:
    void applyCategoryStyle();
    void saveDesktopConfig();

    std::unique_ptr<Ui::SettingsTab> ui;
    QButtonGroup* category_group_ = nullptr;

    Q_DISABLE_COPY_MOVE(SettingsTab)
};

#endif // CLIENT_DESKTOP_SETTINGS_TAB_H
