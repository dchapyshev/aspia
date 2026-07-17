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

#ifndef CLIENT_DESKTOP_CHAT_CHAT_WINDOW_H
#define CLIENT_DESKTOP_CHAT_CHAT_WINDOW_H

#include <QTreeWidget>

#include <memory>

#include "client/desktop/client_window.h"

namespace Ui {
class ChatWindow;
} // namespace Ui

class ChatWindow final : public ClientWindow
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget* parent = nullptr);
    ~ChatWindow() final;

    // ClientWindow implementation.
    void setTabbedMode(bool tabbed) final;
    QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const final;

protected:
    // ClientWindow implementation.
    void onInternalReset() final;
    void onRegisterWorkers() final;
    void onSessionStarted() final;

private slots:
    void onChannelMessage(const QByteArray& buffer);

private:
    std::unique_ptr<Ui::ChatWindow> ui;
    Q_DISABLE_COPY_MOVE(ChatWindow)
};

#endif // CLIENT_DESKTOP_CHAT_CHAT_WINDOW_H
