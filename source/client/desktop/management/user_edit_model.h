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

#ifndef CLIENT_DESKTOP_MANAGEMENT_USER_EDIT_MODEL_H
#define CLIENT_DESKTOP_MANAGEMENT_USER_EDIT_MODEL_H

#include <optional>

#include "base/peer/router_user.h"

// The edit state of a user dialog, with no UI and no networking - unit-testable. The server
// snapshot is written only by applySnapshot() (fed from list replies); the operator intents
// live next to it and never overwrite it, so a failed save retries against the true server
// state and an "unchanged" save can be told from a real edit (see the dialog-snapshot rule).
class UserEditModel
{
public:
    // entry_id == 0 means create mode; > 0 means modify mode.
    explicit UserEditModel(qint64 entry_id);

    qint64 entryId() const { return entry_id_; }
    bool isModifyMode() const { return entry_id_ > 0; }

    //----------------------------------------------------------------------------------------------
    // Server snapshot
    //----------------------------------------------------------------------------------------------

    // |record| is the parsed record of the edited user (ignored in create mode);
    // |record_found| tells whether the reply contained it at all. Returns false in modify mode
    // when the record is gone: it was deleted from another console and the dialog must close.
    bool applySnapshot(const RouterUser& record, bool record_found);

    bool isLoaded() const { return loaded_; }

    // The server snapshot; written only by applySnapshot(). The dialog reads credentials and
    // key material from here when assembling a flags-only request - into a local copy, never
    // back into the model.
    const RouterUser& snapshot() const { return snapshot_; }

    bool snapshotEnabled() const;

    //----------------------------------------------------------------------------------------------
    // Operator intents
    //----------------------------------------------------------------------------------------------

    // The name/password editing began (true) or was reset by the initial load (false). While
    // set, a refetch must not touch the name field.
    void setAccountChanged(bool changed) { account_changed_ = changed; }
    bool accountChanged() const { return account_changed_; }

    // The operator toggled the enabled checkbox. An intent equal to the snapshot state clears
    // itself: an undone edit is no edit, and the checkbox re-attaches to the refetches -
    // otherwise a click-and-undo would silently overwrite a later concurrent change.
    void setEnabledIntent(bool enabled);

    // What the checkbox must show: the operator intent while one is set, the server state
    // otherwise.
    bool desiredEnabled() const;

    bool enabledTouched() const { return enabled_intent_.has_value(); }

    //----------------------------------------------------------------------------------------------
    // Save
    //----------------------------------------------------------------------------------------------

    // A modify save with nothing edited must not echo the snapshot back: between the fetch and
    // the click another console could have changed the record (e.g. disabled the user), and
    // even an "unchanged" save would silently overwrite that.
    bool isNoOpSave() const;

    quint32 flagsForSave() const;

private:
    const qint64 entry_id_;

    RouterUser snapshot_;
    std::optional<bool> enabled_intent_;
    bool account_changed_ = true; // Create mode edits the account by definition.
    bool loaded_ = false;
};

#endif // CLIENT_DESKTOP_MANAGEMENT_USER_EDIT_MODEL_H
