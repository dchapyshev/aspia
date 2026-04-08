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

namespace client {

//--------------------------------------------------------------------------------------------------
ClientTab::ClientTab(Type type, const QString& object_name, QWidget* parent)
    : QWidget(parent),
      type_(type)
{
    setAutoFillBackground(true);
    setObjectName(object_name);
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
const QList<ClientTab::ActionGroupEntry>& ClientTab::actionGroups() const
{
    return action_groups_;
}

//--------------------------------------------------------------------------------------------------
void ClientTab::addActions(ActionGroup group, const QList<QAction*>& actions)
{
    action_groups_.append({ group, actions });
}

} // namespace client
