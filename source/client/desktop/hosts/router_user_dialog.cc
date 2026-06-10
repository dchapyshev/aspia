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

#include "client/desktop/hosts/router_user_dialog.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QLocale>
#include <QPushButton>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/router.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "proto/router.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_user_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterUserDialog::RouterUserDialog(qint64 router_id, qint64 user_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterUserDialog>()),
      router_id_(router_id),
      entry_id_(user_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->checkbox_disable->setChecked(false);

    auto add_session = [&](proto::router::SessionType session_type)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();

        item->setText(0, sessionTypeToString(session_type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, Qt::UserRole, QVariant(session_type));

        if (entry_id_ == 0 && session_type == proto::router::SESSION_TYPE_CLIENT)
            item->setCheckState(0, Qt::Checked);
        else
            item->setCheckState(0, Qt::Unchecked);

        ui->tree_sessions->addTopLevelItem(item);
    };

    add_session(proto::router::SESSION_TYPE_ADMIN);
    add_session(proto::router::SESSION_TYPE_MANAGER);
    add_session(proto::router::SESSION_TYPE_CLIENT);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterUserDialog::onButtonBoxClicked);
    connect(ui->edit_username, &QLineEdit::textEdited, this, [this]()
    {
        setAccountChanged(true);
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
    existing_names_.clear();
    tokens_.clear();

    for (int i = 0; i < list.user_size(); ++i)
    {
        const proto::router::User& user = list.user(i);

        if (entry_id_ > 0 && user.entry_id() == entry_id_)
        {
            // The user we are editing - populate the form. Its name is excluded from
            // existing_names_ so the uniqueness check does not flag the unchanged name.
            user_ = RouterUser::parseFrom(user);

            ui->checkbox_disable->setChecked(!(user_.flags & User::ENABLED));
            ui->edit_username->setText(user_.name);
            ui->button_reset_otp->setVisible(user.otp_active());

            for (int j = 0; j < ui->tree_sessions->topLevelItemCount(); ++j)
            {
                QTreeWidgetItem* item = ui->tree_sessions->topLevelItem(j);
                const quint32 session_type = item->data(0, Qt::UserRole).toUInt();
                item->setCheckState(0,
                    (user_.sessions & session_type) ? Qt::Checked : Qt::Unchecked);
            }

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

            setAccountChanged(false);
        }
        else
        {
            existing_names_.append(QString::fromStdString(user.name()));
        }
    }

    users_loaded_ = true;
    updateTokenTree();
    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::onUserResultReceived(const proto::router::UserResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] User saved";
        accept();
        close();
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

    if (account_changed_)
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

        for (QStringList::size_type i = 0; i < existing_names_.size(); ++i)
        {
            if (username.compare(existing_names_.at(i), Qt::CaseInsensitive) == 0)
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
        user_ = RouterUser::create(username, password);
        user_.entry_id = entry_id_;

        if (!user_.isValid())
        {
            LOG(ERROR) << "Unable to create user";
            MsgBox::warning(this, tr("Unknown internal error when creating or modifying a user."));
            return;
        }
    }

    quint32 sessions = 0;
    for (int i = 0; i < ui->tree_sessions->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = ui->tree_sessions->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            sessions |= item->data(0, Qt::UserRole).toUInt();
    }

    quint32 flags = 0;
    if (!ui->checkbox_disable->isChecked())
        flags |= User::ENABLED;

    user_.sessions = sessions;
    user_.flags = flags;

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting user (entry_id:" << entry_id_ << ")";
    if (entry_id_ > 0)
        router->modifyUser(user_.serialize(), this, &RouterUserDialog::onUserResultReceived);
    else
        router->addUser(user_.serialize(), this, &RouterUserDialog::onUserResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::setAccountChanged(bool changed)
{
    account_changed_ = changed;

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
    const bool ready = users_loaded_;

    ui->edit_username->setEnabled(ready);
    ui->checkbox_disable->setEnabled(ready);
    ui->tree_sessions->setEnabled(ready);
    ui->edit_password->setEnabled(ready && account_changed_);
    ui->edit_password_retry->setEnabled(ready && account_changed_);

    if (QPushButton* ok_button = ui->buttonbox->button(QDialogButtonBox::Ok))
        ok_button->setEnabled(ready);
}

//--------------------------------------------------------------------------------------------------
void RouterUserDialog::updateTokenTree()
{
    ui->tree_tokens->clear();

    for (const Token& token : std::as_const(tokens_))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, formatTimestamp(token.created_at));
        item->setText(1, formatTimestamp(token.last_used_at));
        item->setText(2, token.address);
        item->setData(0, Qt::UserRole, QVariant::fromValue(token.token_id));
        ui->tree_tokens->addTopLevelItem(item);
    }

    ui->button_revoke_token->setEnabled(false);
    ui->button_revoke_all_tokens->setEnabled(!tokens_.isEmpty());
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
