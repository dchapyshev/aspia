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

#include "client/desktop/management/user_edit_model.h"

#include "base/peer/user.h"

//--------------------------------------------------------------------------------------------------
UserEditModel::UserEditModel(qint64 entry_id)
    : entry_id_(entry_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool UserEditModel::applySnapshot(
    const RouterUser& record, bool record_found, const QStringList& other_names)
{
    other_names_ = other_names;

    if (isModifyMode())
    {
        if (!record_found)
            return false;

        snapshot_ = record;

        // The intent tracks the difference to the snapshot; the snapshot moved, so re-evaluate:
        // an intent that now matches the server state dissolves into "no edit".
        if (enabled_intent_.has_value() && *enabled_intent_ == snapshotEnabled())
            enabled_intent_.reset();
    }

    loaded_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool UserEditModel::snapshotEnabled() const
{
    return (snapshot_.flags & User::ENABLED) != 0;
}

//--------------------------------------------------------------------------------------------------
void UserEditModel::setEnabledIntent(bool enabled)
{
    if (isModifyMode() && enabled == snapshotEnabled())
    {
        enabled_intent_.reset();
        return;
    }

    enabled_intent_ = enabled;
}

//--------------------------------------------------------------------------------------------------
bool UserEditModel::desiredEnabled() const
{
    if (enabled_intent_.has_value())
        return *enabled_intent_;
    if (isModifyMode())
        return snapshotEnabled();
    return true; // A new user starts enabled.
}

//--------------------------------------------------------------------------------------------------
bool UserEditModel::isNoOpSave() const
{
    return isModifyMode() && !account_changed_ && desiredEnabled() == snapshotEnabled();
}

//--------------------------------------------------------------------------------------------------
quint32 UserEditModel::flagsForSave() const
{
    quint32 flags = 0;
    if (desiredEnabled())
        flags |= User::ENABLED;
    return flags;
}
