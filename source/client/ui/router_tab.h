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

#ifndef CLIENT_UI_ROUTER_TAB_H
#define CLIENT_UI_ROUTER_TAB_H

#include "client/ui/client_tab.h"

namespace client {

class RouterTab : public ClientTab
{
    Q_OBJECT

public:
    explicit RouterTab(QWidget* parent = nullptr);
    ~RouterTab() override;

    void onActivated(QToolBar* toolbar, QStatusBar* statusbar) override;
    void onDeactivated(QToolBar* toolbar, QStatusBar* statusbar) override;

private:
    Q_DISABLE_COPY_MOVE(RouterTab)
};

} // namespace client

#endif // CLIENT_UI_ROUTER_TAB_H
