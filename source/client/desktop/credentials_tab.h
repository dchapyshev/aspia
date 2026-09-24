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

#ifndef CLIENT_DESKTOP_CREDENTIALS_TAB_H
#define CLIENT_DESKTOP_CREDENTIALS_TAB_H

#include <memory>

#include "client/desktop/tab.h"

namespace Ui {
class CredentialsTab;
} // namespace Ui

class CredentialConfig;
class CredentialListModel;

class CredentialsTab final : public Tab
{
    Q_OBJECT

public:
    explicit CredentialsTab(QWidget* parent = nullptr);
    ~CredentialsTab() final;

    // Tab implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;
    bool hasStatusBar() const final;

private slots:
    void onExportAction();
    void onImportAction();
    void onAddAction();
    void onEditAction();
    void onDeleteAction();
    void onSelectionChanged();
    void onContextMenu(const QPoint& pos);

private:
    void reload(qint64 credential_id);
    const CredentialConfig* selectedCredential() const;

    std::unique_ptr<Ui::CredentialsTab> ui;
    CredentialListModel* model_ = nullptr;

    Q_DISABLE_COPY_MOVE(CredentialsTab)
};

#endif // CLIENT_DESKTOP_CREDENTIALS_TAB_H
