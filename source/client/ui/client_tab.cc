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

#include "client/ui/client_tab.h"

#include <QToolBar>

namespace client {

//--------------------------------------------------------------------------------------------------
ClientTab::ClientTab(Type type, QWidget* parent)
    : QWidget(parent),
      type_(type)
{
    setAutoFillBackground(true);
}

//--------------------------------------------------------------------------------------------------
ClientTab::~ClientTab() = default;

//--------------------------------------------------------------------------------------------------
ClientTab::Type ClientTab::tabType() const
{
    return type_;
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::isClosable() const
{
    return type_ == Type::SESSION;
}

//--------------------------------------------------------------------------------------------------
bool ClientTab::hasSearchField() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
void ClientTab::onSearchTextChanged(const QString& /* text */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ClientTab::addToolbarAction(QToolBar* toolbar, QAction* action, QAction* before)
{
    toolbar->insertAction(before, action);
    toolbar_actions_.append(action);
}

//--------------------------------------------------------------------------------------------------
void ClientTab::removeAllToolbarActions(QToolBar* toolbar)
{
    for (QAction* action : std::as_const(toolbar_actions_))
        toolbar->removeAction(action);

    toolbar_actions_.clear();
}

} // namespace client
