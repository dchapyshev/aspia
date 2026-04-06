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

#include "client/ui/session_tab.h"

#include <QLabel>
#include <QVBoxLayout>

namespace client {

//--------------------------------------------------------------------------------------------------
SessionTab::SessionTab(const QString& title, QWidget* parent)
    : ClientTab(Type::SESSION, parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* label = new QLabel(title, this);
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
}

//--------------------------------------------------------------------------------------------------
SessionTab::~SessionTab() = default;

//--------------------------------------------------------------------------------------------------
void SessionTab::onActivated(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SessionTab::onDeactivated(QStatusBar* /* statusbar */)
{
    // Nothing
}

} // namespace client
