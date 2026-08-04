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

#include "client/desktop/management/router_user_dialog.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QLocale>
#include <QPushButton>
#include <QScrollBar>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/router.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "proto/router.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_user_dialog.h"

namespace {

// The built-in user created by the router's --create-config. The router refuses to delete it or to
// disable it, because it is the only guaranteed way into the admin channel. Mirrored here so the
// dialog does not offer a change that will be rejected.
constexpr qint64 kBuiltInUserId = 1;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterUserDialog::RouterUserDialog(qint64 router_id, qint64 user_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterUserDialog>()),
      router_id_(router_id),
      entry_id_(user_id),
      model_(user_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->checkbox_disable->setChecked(false);

    // The higher levels imply the lower ones (an administrator can also act as a manager and a
    // client, a manager as a client), so a single level is stored instead of a set of types.
    auto add_level = [&](proto::router::SessionType session_type)
    {
        ui->combo_access_level->addItem(sessionTypeToString(session_type), QVariant(session_type));
    };

    add_level(proto::router::SESSION_TYPE_ADMIN);
    add_level(proto::router::SESSION_TYPE_MANAGER);
    add_level(proto::router::SESSION_TYPE_CLIENT);

    if (entry_id_ == 0)
        setAccessLevel(proto::router::SESSION_TYPE_CLIENT);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterUserDialog::onButtonBoxClicked);
    connect(ui->edit_username, &QLineEdit::textEdited, this, [this]()
    {
        setAccountChanged(true);
    });

    // clicked() fires only on operator interaction, not on the programmatic setChecked of the
    // list handler - exactly the boundary between an edit and the snapshot the model relies on.
    connect(ui->checkbox_disable, &QCheckBox::clicked, this, [this]()
    {
        model_.setEnabledIntent(!ui->checkbox_disable->isChecked());
    });

    connect(ui->button_reset_otp, &QPushButton::clicked,
            this, &RouterUserDialog::onResetOtpClicked);

    connect(ui->button_revoke_token, &QPushButton::clicked,
            this, &RouterUserDialog::onRevokeTokenClicked);
    connect(ui->button_revoke_all_tokens, &QPushButton::clicked,
            this, &RouterUserDialog::onRevokeAllTokensClicked);
    connect(ui->tree_tokens, &QTreeWidget::itemSelectionChanged,
            this, &RouterUserDialog::onTokenSelectionChanged);

    // Tokens are device credentials owned by an existing user; in create mode there is nothing
    // to show and nothing to revoke until the user is persisted.
    if (entry_id_ == 0)
        ui->tab_widget->setTabEnabled(ui->tab_widget->indexOf(ui->tab_sessions), false);

    updateTokenTree();
    updateLoadingState();

    Router* router = Router::instance(router_id_);
    CHECK(router);

    connect(router, &Router::sig_statusChanged, this, [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            reject();
    });

    // The record can be changed from another console while the dialog is open (e.g. the user is
    // disabled); without a refetch a later OK would write the stale snapshot back and silently
    // undo that. The fields the operator has touched keep their edits (see onUserListReceived).
    connect(router, &Router::sig_usersChanged, this, [this](qint64 /* router_id */)
    {
        Router* router = Router::instance(router_id_);
        if (router)
            router->listUsers(this, &RouterUserDialog::onUserListReceived);
    });

    router->listUsers(this, &RouterUserDialog::onUserListReceived);
}

//--------------------------------------------------------------------------------------------------
RouterUserDialog::~RouterUserDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool RouterUserDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick &&
        (object == ui->edit_password || object == ui->edit_password_retry))
    {
        setAccountChanged(true);

        if (object == ui->edit_password)
            ui->edit_password->setFocus();
        else if (object == ui->edit_password_retry)
            ui->edit_password_retry->setFocus();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onUserListReceived(const proto::router::UserList& list)
{
    if (list.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the list of the users:" << list.error_code();
        if (!model_.isLoaded() && !closing_)
        {
            // Without the list the dialog is unusable: the uniqueness check has no names, and in
            // modify mode the form is never populated. Tell the operator and close instead of
            // presenting an empty form as if it were real. closing_ collapses several failed
            // replies (the ctor fetch plus refetches) into one message and one reject.
            closing_ = true;
            MsgBox::warning(this, tr("Failed to get list of users."));
            reject();
        }
        // On a refetch keep the current snapshot: a transient error must not close the dialog
        // under the hands of the operator.
        return;
    }

    if (closing_)
        return;

    const bool initial_load = !model_.isLoaded();

    // Split the reply into what the model owns (the record, the names) and what stays display
    // only (tokens, OTP state).
    RouterUser record;
    bool record_found = false;
    bool otp_active = false;
    QStringList other_names;
    tokens_.clear();

    for (int i = 0; i < list.user_size(); ++i)
    {
        const proto::router::User& user = list.user(i);

        if (entry_id_ > 0 && user.entry_id() == entry_id_)
        {
            record_found = true;
            record = RouterUser::parseFrom(user);
            otp_active = user.otp_active();

            tokens_.reserve(user.token_size());
            for (int j = 0; j < user.token_size(); ++j)
            {
                const proto::router::User::Token& src = user.token(j);
                Token token;
                token.token_id     = src.token_id();
                token.created_at   = src.created_at();
                token.last_used_at = src.last_used_at();
                token.address      = QString::fromStdString(src.address());
                tokens_.append(token);
            }
        }
        else
        {
            // The own name is excluded so the uniqueness check does not flag the unchanged name.
            other_names.append(QString::fromStdString(user.name()));
        }
    }

    if (!model_.applySnapshot(record, record_found, other_names))
    {
        // The record being edited is gone (deleted from another console). Every further action
        // would fail with NotFound - or worse, close as a no-op over a nonexistent record - so
        // the dialog closes right away.
        LOG(ERROR) << "Edited user" << entry_id_ << "no longer exists";
        closing_ = true;
        MsgBox::warning(this, tr("The user was deleted from another console."));
        reject();
        return;
    }

    if (entry_id_ > 0)
    {
        // The widgets mirror the model: the checkbox shows the operator intent while one is
        // set and the server state otherwise; the name follows the server only while the
        // operator has not started editing the account.
        ui->checkbox_disable->setChecked(!model_.desiredEnabled());
        if (!model_.accountChanged() || initial_load)
            ui->edit_username->setText(model_.snapshot().name);
        ui->button_reset_otp->setVisible(otp_active);

        if (initial_load)
        {
            // The access level never changes after creation (see I1 in router/database.h)
            // and the password fields must not be reset by a background refetch.
            setAccessLevel(accessLevelFromSessions(model_.snapshot().sessions));
            setAccountChanged(false);
        }
    }

    updateTokenTree();
    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onUserResultReceived(const proto::router::UserResult& result)
{
    // The dialog is already going away (a refetch found the record deleted, or the list never
    // loaded) - a late save result must not stack another message box on top.
    if (closing_)
        return;

    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] User saved";
        accept();
        close();
        return;
    }

    if (error_code == proto::router::kErrorConflict)
    {
        // The workspace keys of the request were sealed from a stale list - a workspace appeared
        // (or its key changed) after this console read it. Reloading the list refreshes the
        // cached keys, so the operator can simply submit again.
        LOG(ERROR) << "User save rejected: concurrent change";
        Router* router = Router::instance(router_id_);
        if (router)
        {
            router->listWorkspaces(Router::CachePolicy::RELOAD, 0, this,
                                   [](const Router::WorkspaceList&) {});
        }
        setEnabled(true);
        MsgBox::warning(this, tr("The router data was changed from another console. The data "
                                 "is being refreshed - please try again."));
        return;
    }

    const char* message;
    if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TR_NOOP("Invalid user request.");
    else if (error_code == proto::router::kErrorInternalError)
        message = QT_TR_NOOP("Unknown internal error.");
    else if (error_code == proto::router::kErrorInvalidData)
        message = QT_TR_NOOP("Invalid data was passed.");
    else if (error_code == proto::router::kErrorAlreadyExists)
        message = QT_TR_NOOP("A user with the specified name already exists.");
    else if (error_code == proto::router::kErrorNotFound)
        message = QT_TR_NOOP("User not found. The list may be out of date.");
    else if (error_code == proto::router::kErrorAccessDenied)
        message = QT_TR_NOOP("This change is not allowed for the selected user.");
    else
        message = QT_TR_NOOP("Unknown error type.");

    LOG(ERROR) << "User save failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, tr(message));
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onResetOtpClicked()
{
    if (entry_id_ <= 0)
        return;

    if (MsgBox::question(this, tr("Resetting two-factor authentication will sign this user out "
                                  "of all sessions and force them to enroll again on next login. "
                                  "Continue?")) != MsgBox::Yes)
    {
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    ui->button_reset_otp->setEnabled(false);
    LOG(INFO) << "[ACTION] Resetting OTP for user" << entry_id_;
    router->resetUserOtp(entry_id_, this, &RouterUserDialog::onResetOtpResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onResetOtpResultReceived(const proto::router::UserResult& result)
{
    ui->button_reset_otp->setEnabled(true);

    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        // Side effect on the router side: every device token of the user was revoked too.
        tokens_.clear();
        updateTokenTree();
        ui->button_reset_otp->setVisible(false);
        return;
    }

    LOG(ERROR) << "OTP reset failed:" << error_code;
    MsgBox::warning(this, tr("Unknown internal error."));
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onRevokeTokenClicked()
{
    QTreeWidgetItem* item = ui->tree_tokens->currentItem();
    if (!item)
        return;

    const qint64 token_id = item->data(0, Qt::UserRole).toLongLong();
    if (token_id <= 0)
        return;

    if (MsgBox::question(this, tr("Are you sure you want to sign this user out of this session?"))
        != MsgBox::Yes)
    {
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    pending_revoke_token_ids_ = { token_id };
    ui->tab_sessions->setEnabled(false);
    LOG(INFO) << "[ACTION] Revoking device token" << token_id << "of user" << entry_id_;
    router->revokeUserTokens(entry_id_, pending_revoke_token_ids_,
                             this, &RouterUserDialog::onRevokeResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onRevokeAllTokensClicked()
{
    if (tokens_.isEmpty())
        return;

    if (MsgBox::question(this,
                         tr("Are you sure you want to sign this user out of all sessions?"))
        != MsgBox::Yes)
    {
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    // Snapshot the current ids so we can drop them from |tokens_| once the router confirms; the
    // wire request itself carries an empty list which the router interprets as "drop every token
    // of this user" atomically.
    pending_revoke_token_ids_.clear();
    pending_revoke_token_ids_.reserve(tokens_.size());
    for (const Token& token : std::as_const(tokens_))
        pending_revoke_token_ids_.append(token.token_id);

    ui->tab_sessions->setEnabled(false);
    LOG(INFO) << "[ACTION] Revoking all device tokens of user" << entry_id_;
    router->revokeUserTokens(entry_id_, /*token_ids=*/QList<qint64>(),
                             this, &RouterUserDialog::onRevokeResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onRevokeResultReceived(const proto::router::UserResult& result)
{
    const QList<qint64> targets = std::move(pending_revoke_token_ids_);
    pending_revoke_token_ids_.clear();
    ui->tab_sessions->setEnabled(true);

    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        for (qint64 id : std::as_const(targets))
        {
            for (int i = 0; i < tokens_.size(); ++i)
            {
                if (tokens_.at(i).token_id == id)
                {
                    tokens_.removeAt(i);
                    break;
                }
            }
        }
        updateTokenTree();
        return;
    }

    const char* message;
    if (error_code == proto::router::kErrorNotFound)
        message = QT_TR_NOOP("Session not found. The list may be out of date.");
    else if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TR_NOOP("Invalid sign-out request.");
    else
        message = QT_TR_NOOP("Unknown internal error.");

    LOG(ERROR) << "Token revoke failed:" << error_code;
    MsgBox::warning(this, tr(message));
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onTokenSelectionChanged()
{
    ui->button_revoke_token->setEnabled(ui->tree_tokens->currentItem() != nullptr);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
    }

    // Nothing was edited - do not echo the snapshot back: between the fetch and this click
    // another console could have changed the record (e.g. disabled the user), and even an
    // "unchanged" save would silently overwrite that. The model tells a real edit from an
    // unchanged form against the current server snapshot.
    if (model_.isNoOpSave())
    {
        LOG(INFO) << "[ACTION] No changes - closing without a request";
        accept();
        close();
        return;
    }

    // The request is built in a local record: the snapshot inside the model is written only by
    // onUserListReceived. Mutating it here would poison the no-op check above on the retry
    // after a failed save - the intended change would read as "already applied".
    RouterUser request;

    if (model_.accountChanged())
    {
        QString username = ui->edit_username->text();

        if (!User::isValidUserName(username))
        {
            LOG(ERROR) << "Invalid user name:" << username;
            MsgBox::warning(this, tr("The user name can not be empty and can contain only "
                "alphabet characters, numbers and ""_"", ""-"", ""."", ""@"" characters."));
            ui->edit_username->selectAll();
            ui->edit_username->setFocus();
            return;
        }

        const QStringList& existing_names = model_.otherNames();
        for (QStringList::size_type i = 0; i < existing_names.size(); ++i)
        {
            if (username.compare(existing_names.at(i), Qt::CaseInsensitive) == 0)
            {
                LOG(ERROR) << "User name already exists:" << username;
                MsgBox::warning(this, tr("The username you entered already exists."));
                ui->edit_username->selectAll();
                ui->edit_username->setFocus();
                return;
            }
        }

        SecureString password = ui->edit_password->password();

        if (password != ui->edit_password_retry->password())
        {
            LOG(INFO) << "Passwords do not match";
            MsgBox::warning(this, tr("The passwords you entered do not match."));
            ui->edit_password->selectAll();
            ui->edit_password->setFocus();
            return;
        }

        if (!User::isValidPassword(password))
        {
            LOG(INFO) << "Invalid password";
            MsgBox::warning(this, tr("Password can not be empty and should not exceed %n characters.",
                "", User::kMaxPasswordLength));

            ui->edit_password->selectAll();
            ui->edit_password->setFocus();
            return;
        }

        if (!User::isSafePassword(password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                "numbers and should not be shorter than %n characters.",
                "", User::kSafePasswordLength);

            QString question = tr("Do you want to enter a different password?");

            MsgBox message_box(MsgBox::Warning,
                                    tr("Warning"),
                                    QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                    MsgBox::Yes | MsgBox::No,
                                    this);
            if (message_box.exec() == MsgBox::Yes)
            {
                ui->edit_password->clear();
                ui->edit_password_retry->clear();
                ui->edit_password->setFocus();
                return;
            }
        }

        // Create new user (regenerates keys). entry_id is preserved for modify mode.
        request = RouterUser::create(username, password);
        request.entry_id = entry_id_;

        if (!request.isValid())
        {
            LOG(ERROR) << "Unable to create user";
            MsgBox::warning(this, tr("Unknown internal error when creating or modifying a user."));
            return;
        }
    }
    else
    {
        // The credentials were not changed - do not echo the snapshot back. It could be out of
        // date (the user rotated the password after this dialog was opened), and the router would
        // treat the stale salt/verifier as a password change and revert the rotation. Empty
        // fields keep the stored values; public_key stays for the workspace keys to be sealed.
        request = model_.snapshot();
        request.salt.clear();
        request.verifier.clear();
        request.wrap_private_key.clear();
        request.wrap_salt.clear();
    }

    // Only the selected level is stored; the router expands it to the implied lower levels.
    request.sessions = ui->combo_access_level->currentData().toUInt();
    request.flags = model_.flagsForSave();

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting user (entry_id:" << entry_id_ << ")";
    if (entry_id_ > 0)
        router->modifyUser(request.serialize(), this, &RouterUserDialog::onUserResultReceived);
    else
        router->addUser(request.serialize(), this, &RouterUserDialog::onUserResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::setAccountChanged(bool changed)
{
    model_.setAccountChanged(changed);

    ui->edit_password->setEnabled(changed);
    ui->edit_password_retry->setEnabled(changed);

    ui->edit_password->clear();
    ui->edit_password_retry->clear();

    ui->edit_password->setShowPassword(!changed);
    ui->edit_password_retry->setShowPassword(!changed);

    if (changed)
    {
        ui->edit_password->setPlaceholderText(QString());
        ui->edit_password_retry->setPlaceholderText(QString());
    }
    else
    {
        QString prompt = tr("Double-click to change");
        ui->edit_password->setPlaceholderText(prompt);
        ui->edit_password_retry->setPlaceholderText(prompt);

        ui->edit_password->installEventFilter(this);
        ui->edit_password_retry->installEventFilter(this);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::updateLoadingState()
{
    const bool ready = model_.isLoaded();

    // The built-in user cannot be disabled; its name and password remain editable.
    const bool built_in = entry_id_ == kBuiltInUserId;

    ui->edit_username->setEnabled(ready);
    ui->checkbox_disable->setEnabled(ready && !built_in);

    // The access level is chosen when the user is created and is not editable afterwards.
    ui->combo_access_level->setEnabled(ready && entry_id_ == 0);

    ui->edit_password->setEnabled(ready && model_.accountChanged());
    ui->edit_password_retry->setEnabled(ready && model_.accountChanged());

    if (QPushButton* ok_button = ui->buttonbox->button(QDialogButtonBox::Ok))
        ok_button->setEnabled(ready);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::updateTokenTree()
{
    // A rebuild also runs on every refetch (batched notifications arrive every few seconds
    // while another console is active), so the selection and the scroll position are carried
    // over - same reasoning as the lists of the workspace dialog.
    QVariant selected_token;
    if (QTreeWidgetItem* current = ui->tree_tokens->currentItem())
        selected_token = current->data(0, Qt::UserRole);
    const int scroll = ui->tree_tokens->verticalScrollBar()->value();

    ui->tree_tokens->clear();

    for (const Token& token : std::as_const(tokens_))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, formatTimestamp(token.created_at));
        item->setText(1, formatTimestamp(token.last_used_at));
        item->setText(2, token.address);
        item->setData(0, Qt::UserRole, QVariant::fromValue(token.token_id));
        ui->tree_tokens->addTopLevelItem(item);

        if (selected_token.isValid() && item->data(0, Qt::UserRole) == selected_token)
            ui->tree_tokens->setCurrentItem(item);
    }

    ui->tree_tokens->verticalScrollBar()->setValue(scroll);

    ui->button_revoke_token->setEnabled(ui->tree_tokens->currentItem() != nullptr);
    ui->button_revoke_all_tokens->setEnabled(!tokens_.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::setAccessLevel(proto::router::SessionType session_type)
{
    int index = ui->combo_access_level->findData(QVariant(session_type));
    if (index >= 0)
        ui->combo_access_level->setCurrentIndex(index);
}

//--------------------------------------------------------------------------------------------------
// static
proto::router::SessionType RouterUserDialog::accessLevelFromSessions(quint32 sessions)
{
    if (sessions & proto::router::SESSION_TYPE_ADMIN)
        return proto::router::SESSION_TYPE_ADMIN;
    if (sessions & proto::router::SESSION_TYPE_MANAGER)
        return proto::router::SESSION_TYPE_MANAGER;
    return proto::router::SESSION_TYPE_CLIENT;
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterUserDialog::sessionTypeToString(proto::router::SessionType session_type)
{
    const char* str = nullptr;

    switch (session_type)
    {
        case proto::router::SESSION_TYPE_ADMIN:
            str = QT_TR_NOOP("Administrator");
            break;

        case proto::router::SESSION_TYPE_MANAGER:
            str = QT_TR_NOOP("Manager");
            break;

        case proto::router::SESSION_TYPE_CLIENT:
            str = QT_TR_NOOP("Client");
            break;

        default:
            break;
    }

    if (!str)
        return QString();

    return tr(str);
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterUserDialog::formatTimestamp(qint64 unix_seconds)
{
    if (unix_seconds <= 0)
        return tr("Never");

    return QLocale().toString(QDateTime::fromSecsSinceEpoch(unix_seconds), QLocale::ShortFormat);
}
