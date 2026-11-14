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
#include <optional>

#include "client/config.h"
#include "client/desktop/tab.h"

namespace Ui {
class SettingsTab;
} // namespace Ui

class QButtonGroup;
class QLineEdit;

class SettingsTab final : public Tab
{
    Q_OBJECT

public:
    explicit SettingsTab(QWidget* parent = nullptr);
    ~SettingsTab() final;

    // Tab implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate() final;
    void deactivate() final;
    bool confirmClose() final;

signals:
    void sig_desktopConfigChanged();
    void sig_lockTimeoutChanged();
    void sig_routerChanged();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onCategoryChanged(int id);
    void onGeneralChanged();
    void onRouterChanged();
    void onSelectBackupPath();
    void onProxyTypeChanged();
    void onAutoUnlockToggled();
    void onSaveGeneral();
    void onProfileChanged();
    void onAddProfile();
    void onRenameProfile();
    void onDeleteProfile();
    void onDesktopFeatureChanged();
    void onSelectRecordingPath();
    void onMcpEnabledChanged();
    void onMcpAutoConfirmChanged();
    void onMcpSignInChanged();
    void onMcpSessionTypesChanged();
    void onCopyMcpCommand();
    void onChangeMasterPassword();
    void onCheckUpdatesChanged();
    void onUpdateChannelChanged();
    void onCheckForUpdatesClicked();
    void onUpdateServerClicked();

private:
    void loadUpdateServer();
    void updateProxyFields();
    void applyCategoryStyle();
    bool enableAutoUnlock();
    void loadRouter();
    bool routerFromUi(std::optional<RouterConfig>* router);
    bool saveRouter(RouterConfig router);
    void showRouterError(QLineEdit* edit, const QString& message);
    void reloadProfiles(qint64 profile_id);
    void showProfile();
    ProfileConfig profileFromUi() const;
    QString askProfileName(const QString& title, const QString& name, int own_index);
    void saveProfile();

    std::unique_ptr<Ui::SettingsTab> ui;
    QButtonGroup* category_group_ = nullptr;
    QButtonGroup* mcp_session_types_ = nullptr;
    bool router_changed_ = false;

    Q_DISABLE_COPY_MOVE(SettingsTab)
};

#endif // CLIENT_DESKTOP_SETTINGS_TAB_H
