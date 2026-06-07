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

#include "client/ui/tab.h"

//--------------------------------------------------------------------------------------------------
Tab::Tab(Type type, const QString& object_name, QWidget* parent)
    : QWidget(parent),
      type_(type)
{
    setAutoFillBackground(true);
    setObjectName(object_name);
}

//--------------------------------------------------------------------------------------------------
Tab::~Tab() = default;

//--------------------------------------------------------------------------------------------------
Tab::Type Tab::tabType() const
{
    return type_;
}

//--------------------------------------------------------------------------------------------------
bool Tab::isClosable() const
{
    return type_ == Type::SESSION || type_ == Type::SETTINGS;
}

//--------------------------------------------------------------------------------------------------
bool Tab::isVisibleBeforeFullscreen() const
{
    return visible_before_fullscreen_;
}

//--------------------------------------------------------------------------------------------------
void Tab::setVisibleBeforeFullscreen(bool was_visible)
{
    visible_before_fullscreen_ = was_visible;
}

//--------------------------------------------------------------------------------------------------
bool Tab::isDetachable() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
bool Tab::isDetached() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
void Tab::detachToWindow()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Tab::attachToTab()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QWidget* Tab::detachedWindow() const
{
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
bool Tab::hasStatusBar() const
{
    return true;
}

//--------------------------------------------------------------------------------------------------
QString Tab::searchText() const
{
    return QString();
}

//--------------------------------------------------------------------------------------------------
void Tab::searchTextChanged(const QString& /* text */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QList<Tab::ActionGroupEntry> Tab::actionGroups() const
{
    return action_groups_;
}

//--------------------------------------------------------------------------------------------------
void Tab::addActions(ActionRole group, const QList<QAction*>& actions)
{
    action_groups_.append({ group, actions });
}
