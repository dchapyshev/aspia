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

#include "client/ui/router_tab.h"

#include <QLabel>
#include <QVBoxLayout>

namespace client {

//--------------------------------------------------------------------------------------------------
RouterTab::RouterTab(QWidget* parent)
    : ClientTab(Type::ROUTER, parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label = new QLabel(tr("Router functionality is under development"), this);
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
}

//--------------------------------------------------------------------------------------------------
RouterTab::~RouterTab() = default;

//--------------------------------------------------------------------------------------------------
void RouterTab::onActivated(QToolBar* /* toolbar */, QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RouterTab::onDeactivated(QToolBar* /* toolbar */, QStatusBar* /* statusbar */)
{
    // Nothing
}

} // namespace client
