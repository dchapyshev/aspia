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

#include "host/android/settings_widget.h"

#include <QByteArray>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <optional>

#include "base/crypto/secure_string.h"
#include "base/gui_application.h"
#include "base/net/address.h"
#include "build/build_config.h"
#include "common/android/about_widget.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "host/database.h"
#include "host/user_settings.h"
#include "host/android/password_dialog.h"
#include "host/android/user_editor_widget.h"
#include "host/android/users_widget.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      settings_page_(new ScrollArea()),
      about_page_(new AboutWidget()),
      users_page_(new UsersWidget()),
      editor_page_(new UserEditorWidget()),
      about_button_(new IconButton(":/img/material/info.svg", this)),
      add_user_button_(new IconButton(":/img/material/add_2.svg", this)),
      save_button_(new IconButton(":/img/done.svg", this))
{
    // The sub-page actions live in the app bar; AppBar::setActions() reparents and shows them. Hidden
    // by default so they do not linger in this widget.
    about_button_->hide();
    connect(about_button_, &IconButton::clicked, this, &SettingsWidget::showAbout);

    add_user_button_->hide();
    connect(add_user_button_, &IconButton::clicked, this, [this]() { showUserEditor(0); });

    save_button_->hide();
    connect(save_button_, &IconButton::clicked, this, [this]() { editor_page_->save(); });

    connect(users_page_, &UsersWidget::sig_editUser, this, &SettingsWidget::showUserEditor);

    // A saved user returns to the (reloaded) list.
    connect(editor_page_, &UserEditorWidget::sig_saved, this, &SettingsWidget::showUsers);

    buildSettings();

    stack_->addWidget(settings_page_);
    stack_->addWidget(about_page_);
    stack_->addWidget(users_page_);
    stack_->addWidget(editor_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack_);
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> SettingsWidget::appBarActions() const
{
    if (isEditorPage())
        return { save_button_ };
    if (isUsersPage())
        return { add_user_button_ };
    if (isAboutPage())
        return {};
    return { about_button_ };
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::goBack()
{
    // The user editor is opened from the user list, so it returns there rather than to the settings.
    if (isEditorPage())
    {
        showUsers();
        return;
    }

    if (!isAboutPage() && !isUsersPage())
        return;

    stack_->setCurrentWidget(settings_page_);
    emit sig_titleChanged(QString(), false);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::resetToSettings()
{
    stack_->setCurrentWidget(settings_page_);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::showAbout()
{
    stack_->setCurrentWidget(about_page_);
    emit sig_titleChanged(tr("About"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::showUsers()
{
    users_page_->reload();
    stack_->setCurrentWidget(users_page_);
    emit sig_titleChanged(tr("Users"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::showUserEditor(qint64 entry_id)
{
    editor_page_->load(entry_id);
    stack_->setCurrentWidget(editor_page_);
    emit sig_titleChanged(editor_page_->isEditing() ? tr("Edit User") : tr("Add User"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isAboutPage() const
{
    return stack_->currentWidget() == about_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isUsersPage() const
{
    return stack_->currentWidget() == users_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isEditorPage() const
{
    return stack_->currentWidget() == editor_page_;
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildSettings()
{
    QWidget* content = new QWidget(settings_page_);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    buildInterfaceSection(layout);
    buildSecuritySection(layout);
    buildRouterSection(layout);
    layout->addStretch();

    // setWidget() deletes the previously set content widget.
    settings_page_->setWidget(content);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::addSectionHeader(QVBoxLayout* layout, const QString& text)
{
    if (layout->count() > 0)
        layout->addSpacing(kSectionSpacing);

    layout->addWidget(new Label(text, Label::Role::CAPTION));
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildInterfaceSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Interface"));

    UserSettings settings;

    ComboBox* theme = new ComboBox();
    theme->setLabel(tr("Theme"));
    for (const QString& id : GuiApplication::instance()->availableThemes())
        theme->addItem(GuiApplication::themeName(id), id);
    theme->setCurrentIndex(qMax(0, theme->findData(settings.theme())));
    connect(theme, &QComboBox::currentIndexChanged, this, [theme](int /* index */)
    {
        const QString id = theme->currentData().toString();
        UserSettings().setTheme(id);
        GuiApplication::instance()->setTheme(id);
    });
    layout->addWidget(theme);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildSecuritySection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Security"));

    Button* manage = new Button(tr("Manage users"), Button::Role::FILLED);
    connect(manage, &Button::clicked, this, &SettingsWidget::showUsers);
    layout->addWidget(manage);

    switch (Database::instance().passwordProtectionState())
    {
        case Database::PasswordProtection::ENABLED:
        {
            Button* change = new Button(tr("Change password"), Button::Role::FILLED);
            connect(change, &Button::clicked, this, &SettingsWidget::onChangePassword);
            layout->addWidget(change);

            Button* disable = new Button(tr("Disable password protection"), Button::Role::TEXT);
            disable->setAccentColor(Controls::errorColor());
            connect(disable, &Button::clicked, this, &SettingsWidget::onDisableProtection);
            layout->addWidget(disable);
        }
        break;

        case Database::PasswordProtection::DISABLED:
        {
            Button* enable = new Button(tr("Enable password protection"), Button::Role::FILLED);
            connect(enable, &Button::clicked, this, &SettingsWidget::onEnableProtection);
            layout->addWidget(enable);
        }
        break;

        case Database::PasswordProtection::UNAVAILABLE:
            // The settings storage is not readable; the password controls are omitted.
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildRouterSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Router"));

    Database& db = Database::instance();
    const bool enabled = db.isRouterEnabled();

    Switch* enable = new Switch(tr("Enable the use of a router"));
    enable->setChecked(enabled);
    layout->addWidget(enable);

    LineEdit* address = new LineEdit();
    address->setLabel(tr("Address"));
    address->setText(db.routerAddress().toString());
    address->setEnabled(enabled);
    layout->addWidget(address);

    LineEdit* public_key = new LineEdit();
    public_key->setLabel(tr("Public Key"));
    public_key->setText(QString::fromUtf8(db.routerPublicKey().toHex()));
    public_key->setEnabled(enabled);
    layout->addWidget(public_key);

    Label* hint = new Label(tr("A router is required to connect to a computer if there is no direct "
                               "connection (bypass NAT). Aspia does not provide a public router, but "
                               "you can install your own. You can download the router on the "
                               "<a href=\"https://aspia.org\">official website</a>."),
                            Label::Role::CAPTION);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setOpenExternalLinks(true);
    layout->addWidget(hint);

    connect(enable, &QCheckBox::toggled, this, [address, public_key](bool checked)
    {
        Database::instance().setRouterEnabled(checked);
        address->setEnabled(checked);
        public_key->setEnabled(checked);
    });

    // Values are validated and stored on commit. Invalid input is reverted to the stored value: a
    // modal dialog cannot be shown here because the on-screen keyboard is still up, and a translucent
    // top-level surface aborts the process (QOpenGLContext::makeCurrent) while the surface is invalid.
    connect(address, &QLineEdit::editingFinished, this, [address]()
    {
        Database& db = Database::instance();

        const Address parsed = Address::fromString(address->text().trimmed(), DEFAULT_ROUTER_TCP_PORT);

        QSignalBlocker blocker(address);
        if (parsed.isValid())
        {
            db.setRouterAddress(parsed);
            address->setText(parsed.toString());
        }
        else
        {
            address->setText(db.routerAddress().toString());
        }
    });

    connect(public_key, &QLineEdit::editingFinished, this, [public_key]()
    {
        Database& db = Database::instance();

        const QByteArray key = QByteArray::fromHex(public_key->text().trimmed().toUtf8());

        QSignalBlocker blocker(public_key);
        if (key.size() == 32)
        {
            db.setRouterPublicKey(key);
            public_key->setText(QString::fromUtf8(key.toHex()));
        }
        else
        {
            public_key->setText(QString::fromUtf8(db.routerPublicKey().toHex()));
        }
    });
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onEnableProtection()
{
    std::optional<SecureString> password = PasswordDialog::create(this);
    if (!password)
        return;

    if (!Database::instance().setPassword(*password))
    {
        MessageDialog::info(this, tr("Error"), tr("An error occurred while processing the password."));
        return;
    }

    buildSettings();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onChangePassword()
{
    // Confirm the current password before setting a new one.
    if (!PasswordDialog::verify(this))
        return;

    std::optional<SecureString> password = PasswordDialog::create(this);
    if (!password)
        return;

    if (!Database::instance().setPassword(*password))
    {
        MessageDialog::info(this, tr("Error"), tr("An error occurred while processing the password."));
        return;
    }

    buildSettings();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onDisableProtection()
{
    if (!PasswordDialog::verify(this))
        return;

    Database::instance().clearPassword();
    buildSettings();
}
