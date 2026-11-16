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

#include "client/android/settings_widget.h"

#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPointer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "base/auto_qpointer.h"
#include "base/build_config.h"
#include "base/gui_application.h"
#include "client/android/biometric_gate.h"
#include "client/android/credentials_widget.h"
#include "client/android/master_password_dialog.h"
#include "client/android/master_password_widget.h"
#include "client/android/profiles_widget.h"
#include "client/storage/database.h"
#include "client/storage/master_password.h"
#include "client/storage/system_settings.h"
#include "client/storage/user_settings.h"
#include "common/android/about_widget.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/granted_folders.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "common/android/update_widget.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;
constexpr int kUpdatePublicKeyLength = 64;

//--------------------------------------------------------------------------------------------------
bool isValidUpdateServer(const QString& server)
{
    if (server.isEmpty())
        return true;

    QUrl url(server, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty() &&
           (url.scheme() == "https" || url.scheme() == "http");
}

//--------------------------------------------------------------------------------------------------
bool isValidUpdatePublicKey(const QByteArray& hex)
{
    return hex.size() == kUpdatePublicKeyLength && QByteArray::fromHex(hex).toHex() == hex;
}

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      settings_page_(new ScrollArea()),
      credentials_page_(new CredentialsWidget()),
      profiles_page_(new ProfilesWidget()),
      about_page_(new AboutWidget()),
      update_page_(new UpdateWidget(kClientUpdatePackage)),
      master_password_page_(new MasterPasswordWidget()),
      button_credentials_(new IconButton(":/img/material/key.svg", this)),
      button_about_(new IconButton(":/img/material/info.svg", this))
{
    // The actions live in the app bar; AppBar::setActions() reparents and shows them. Hidden by
    // default so they do not linger in this widget.
    button_credentials_->hide();
    button_about_->hide();
    connect(button_credentials_, &IconButton::clicked, this, &SettingsWidget::onShowCredentials);
    connect(button_about_, &IconButton::clicked, this, &SettingsWidget::onShowAbout);

    connect(credentials_page_, &CredentialsWidget::sig_titleChanged,
            this, &SettingsWidget::onCredentialsTitleChanged);
    connect(credentials_page_, &CredentialsWidget::sig_appBarActionsChanged,
            this, &SettingsWidget::onCredentialsActionsChanged);
    connect(profiles_page_, &ProfilesWidget::sig_titleChanged, this, &SettingsWidget::onProfilesTitleChanged);
    connect(profiles_page_, &ProfilesWidget::sig_appBarActionsChanged,
            this, &SettingsWidget::onProfilesActionsChanged);
    connect(master_password_page_, &MasterPasswordWidget::sig_finished,
            this, &SettingsWidget::onMasterPasswordChanged);

    buildSettings();

    stack_->addWidget(settings_page_);
    stack_->addWidget(credentials_page_);
    stack_->addWidget(profiles_page_);
    stack_->addWidget(about_page_);
    stack_->addWidget(update_page_);
    stack_->addWidget(master_password_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack_);
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> SettingsWidget::appBarActions() const
{
    if (isCredentialsPage())
        return credentials_page_->appBarActions();
    if (isProfilesPage())
        return profiles_page_->appBarActions();
    if (isAboutPage())
        return about_page_->appBarActions();
    if (isUpdatePage() || isMasterPasswordPage())
        return {};
    return { button_credentials_, button_about_ };
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::goBack()
{
    // The editor, the export and the import screens of the credentials screen are pages of their own
    // below the list.
    if (isCredentialsPage() && !credentials_page_->isListPage())
    {
        credentials_page_->goBack();
        return;
    }

    if (isProfilesPage() && !profiles_page_->isListPage())
    {
        profiles_page_->goBack();
        return;
    }

    if (!isCredentialsPage() && !isProfilesPage() && !isAboutPage() && !isUpdatePage() &&
        !isMasterPasswordPage())
    {
        return;
    }

    master_password_page_->clear();
    stack_->setCurrentWidget(settings_page_);
    emit sig_titleChanged(QString(), false);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::resetToSettings()
{
    if (!credentials_page_->isListPage())
        credentials_page_->goBack();
    if (!profiles_page_->isListPage())
        profiles_page_->goBack();

    master_password_page_->clear();
    stack_->setCurrentWidget(settings_page_);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onShowUpdate()
{
    // The button does not take the focus, so a field still being edited is saved here first.
    if (QWidget* widget = focusWidget())
        widget->clearFocus();

    SystemSettings settings;
    update_page_->check(settings.updateServer(), settings.updatePublicKey(), UserSettings().updateChannel());

    stack_->setCurrentWidget(update_page_);
    emit sig_titleChanged(tr("Update"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onCredentialsTitleChanged(const QString& title)
{
    if (isCredentialsPage())
        emit sig_titleChanged(title, true);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onCredentialsActionsChanged()
{
    if (isCredentialsPage())
        emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onProfilesTitleChanged(const QString& title)
{
    if (isProfilesPage())
        emit sig_titleChanged(title, true);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onProfilesActionsChanged()
{
    if (isProfilesPage())
        emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onMasterPasswordChanged()
{
    goBack();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onShowCredentials()
{
    credentials_page_->reload();
    stack_->setCurrentWidget(credentials_page_);
    emit sig_titleChanged(tr("Credentials"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onShowProfiles()
{
    profiles_page_->reload();
    stack_->setCurrentWidget(profiles_page_);
    emit sig_titleChanged(tr("Profiles"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onShowAbout()
{
    stack_->setCurrentWidget(about_page_);
    emit sig_titleChanged(tr("About"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::onAddFolder()
{
    const QUrl url = QFileDialog::getExistingDirectoryUrl(this, tr("Add folder"));
    if (url.isEmpty())
        return;

    if (!GrantedFolders::add(url.toString(QUrl::FullyEncoded)))
    {
        MessageDialog::info(this, tr("File Transfer"),
            tr("This folder cannot be added. Choose a folder in the internal storage or on the SD card."));
        return;
    }

    buildSettings();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::showMasterPassword()
{
    master_password_page_->prepare();
    stack_->setCurrentWidget(master_password_page_);
    emit sig_titleChanged(tr("Change Master Password"), true);
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isCredentialsPage() const
{
    return stack_->currentWidget() == credentials_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isProfilesPage() const
{
    return stack_->currentWidget() == profiles_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isAboutPage() const
{
    return stack_->currentWidget() == about_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isUpdatePage() const
{
    return stack_->currentWidget() == update_page_;
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::isMasterPasswordPage() const
{
    return stack_->currentWidget() == master_password_page_;
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
    buildProfilesSection(layout);
    buildFileTransferSection(layout);
#if !defined(ASPIA_GOOGLE_PLAY)
    buildUpdateSection(layout);
#endif // !defined(ASPIA_GOOGLE_PLAY)
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
void SettingsWidget::addBoolSetting(QVBoxLayout* layout, const QString& text, bool value,
                                    const std::function<void(bool)>& on_changed)
{
    Switch* control = new Switch(text);
    control->setChecked(value);
    connect(control, &Switch::toggled, this, on_changed);
    layout->addWidget(control);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildInterfaceSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Interface"));

    ComboBox* theme = new ComboBox();
    theme->setLabel(tr("Theme"));
    for (const QString& id : GuiApplication::instance()->availableThemes())
        theme->addItem(GuiApplication::themeName(id), id);
    theme->setCurrentIndex(qMax(0, theme->findData(settings_.theme())));
    connect(theme, &QComboBox::currentIndexChanged, this, [this, theme](int /* index */)
    {
        const QString id = theme->currentData().toString();
        settings_.setTheme(id);
        GuiApplication::instance()->setTheme(id);
    });
    layout->addWidget(theme);

    LineEdit* display_name = new LineEdit();
    display_name->setLabel(tr("Display name when connected"));
    display_name->setMaxLength(16);
    display_name->setText(Database::instance().displayName());
    connect(display_name, &QLineEdit::editingFinished, this, [display_name]()
    {
        Database::instance().setDisplayName(display_name->text());
    });
    layout->addWidget(display_name);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildSecuritySection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Security"));

    Button* change_password = new Button(tr("Change Master Password"), Button::Role::FILLED);
    connect(change_password, &Button::clicked, this, [this]() { showMasterPassword(); });
    layout->addWidget(change_password);

    const BiometricGate::Status biometric_status = BiometricGate::status();
    const bool biometric_enabled = Database::instance().isBiometricUnlockEnabled();
    const bool usable = biometric_status == BiometricGate::Status::AVAILABLE || biometric_enabled;

    QPointer<Switch> biometric = new Switch(tr("Unlock with biometrics"));
    biometric->setChecked(biometric_enabled);
    biometric->setEnabled(usable);
    layout->addWidget(biometric);

    if (usable)
    {
        connect(biometric, &Switch::toggled, this, [this, biometric](bool checked)
        {
            if (setBiometricEnabled(checked))
                return;

            // setBiometricEnabled() runs nested event loops (master password and the biometric
            // prompt); the switch may have been destroyed by the time they return.
            if (!biometric)
                return;

            QSignalBlocker blocker(biometric);
            biometric->setChecked(!checked);
        });
    }
    else
    {
        QString hint;
        if (biometric_status == BiometricGate::Status::NONE_ENROLLED)
            hint = tr("Set up a fingerprint in the system settings to use this.");
        else
            hint = tr("Biometrics are not available on this device.");

        Label* label = new Label(hint, Label::Role::CAPTION);
        label->setWordWrap(true);
        layout->addWidget(label);
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildProfilesSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Profiles"));

    Button* manage = new Button(tr("Manage Profiles"), Button::Role::FILLED);
    connect(manage, &Button::clicked, this, &SettingsWidget::onShowProfiles);
    layout->addWidget(manage);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildFileTransferSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("File Transfer"));

    const QList<GrantedFolders::Folder> folders = GrantedFolders::list();
    for (const GrantedFolders::Folder& folder : folders)
    {
        Label* path = new Label(folder.path);
        path->setWordWrap(true);

        IconButton* remove = new IconButton(":/img/material/delete.svg");
        connect(remove, &IconButton::clicked, this, [this, folder]()
        {
            removeFolder(folder.path, folder.uri);
        });

        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(path, 1);
        row->addWidget(remove);
        layout->addLayout(row);
    }

    Button* add = new Button(tr("Add folder"), Button::Role::FILLED);
    connect(add, &Button::clicked, this, &SettingsWidget::onAddFolder);
    layout->addWidget(add);

    Label* hint = new Label(tr("Files can be transferred only in the folders added here."),
                            Label::Role::CAPTION);
    hint->setWordWrap(true);
    layout->addWidget(hint);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildUpdateSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Updates"));

    addBoolSetting(layout, tr("Check for updates on startup"), UserSettings().isCheckUpdatesEnabled(),
                   [](bool checked)
    {
        UserSettings().setCheckUpdatesEnabled(checked);
    });

    ComboBox* channel = new ComboBox();
    channel->setLabel(tr("Update channel"));
    channel->addItem(tr("Stable"), kStableUpdateChannel);
    channel->addItem(tr("Beta"), kBetaUpdateChannel);
    channel->addItem(tr("Alpha"), kAlphaUpdateChannel);
    channel->setCurrentIndex(qMax(0, channel->findData(UserSettings().updateChannel())));
    connect(channel, &QComboBox::currentIndexChanged, this, [channel](int /* index */)
    {
        UserSettings().setUpdateChannel(channel->currentData().toString());
    });
    layout->addWidget(channel);

    SystemSettings settings;

    LineEdit* server = new LineEdit();
    server->setLabel(tr("Update server"));
    server->setText(settings.updateServer());
    layout->addWidget(server);

    LineEdit* public_key = new LineEdit();
    public_key->setLabel(tr("Public key"));
    public_key->setMaxLength(kUpdatePublicKeyLength);
    public_key->setText(QString::fromLatin1(settings.updatePublicKey().toHex()));
    public_key->setEnabled(!settings.updateServer().isEmpty());
    layout->addWidget(public_key);

    Label* hint = new Label(tr("If the update server is not specified, the default one is used. "
                               "If the public key is not specified, the built-in one is used."),
                            Label::Role::CAPTION);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(server, &QLineEdit::textChanged, public_key, [server, public_key]()
    {
        public_key->setEnabled(!server->text().trimmed().isEmpty());
    });

    connect(server, &QLineEdit::editingFinished, this, [server, public_key]()
    {
        SystemSettings settings;
        const QString value = server->text().trimmed();

        QSignalBlocker blocker(server);
        if (!isValidUpdateServer(value))
        {
            server->setText(settings.updateServer());
            public_key->setEnabled(!settings.updateServer().isEmpty());
            return;
        }

        settings.setUpdateServer(value);
        server->setText(value);

        if (value.isEmpty())
        {
            settings.setUpdatePublicKey(QByteArray());
            public_key->clear();
        }

        settings.sync();
    });

    connect(public_key, &QLineEdit::editingFinished, this, [public_key]()
    {
        SystemSettings settings;
        const QByteArray value = public_key->text().trimmed().toLower().toLatin1();

        QSignalBlocker blocker(public_key);
        if (value.isEmpty() || isValidUpdatePublicKey(value))
        {
            settings.setUpdatePublicKey(QByteArray::fromHex(value));
            settings.sync();
            public_key->setText(QString::fromLatin1(value));
        }
        else
        {
            public_key->setText(QString::fromLatin1(settings.updatePublicKey().toHex()));
        }
    });

    Button* check = new Button(tr("Check for updates"), Button::Role::FILLED);
    connect(check, &Button::clicked, this, &SettingsWidget::onShowUpdate);
    layout->addWidget(check);
}

//--------------------------------------------------------------------------------------------------
bool SettingsWidget::setBiometricEnabled(bool enable)
{
    Database& db = Database::instance();

    if (!enable)
    {
        db.clearBiometricUnlock();
        BiometricGate::deleteKey();
        return true;
    }

    // Confirm the master password before binding its key to a biometric, so a momentarily unlocked
    // app cannot be used to enroll someone else's fingerprint.
    AutoQPointer<MasterPasswordDialog> dialog(new MasterPasswordDialog(MasterPasswordDialog::Mode::UNLOCK, this));
    if (dialog->exec() != QDialog::Accepted)
        return false;

    BiometricGate::Prompt prompt;
    prompt.title = tr("Enable biometric unlock");
    prompt.negative_text = tr("Cancel");

    std::optional<BiometricGate::Blob> blob =
        BiometricGate::enroll(MasterPassword::currentKey(), prompt);
    if (!blob.has_value())
        return false;

    return db.setBiometricBlob(BiometricGate::pack(*blob));
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::removeFolder(const QString& path, const QString& uri)
{
    if (!MessageDialog::confirm(this, tr("File Transfer"),
            tr("Remove the folder %1 from file transfer?").arg(path), tr("Remove")))
    {
        return;
    }

    GrantedFolders::remove(uri);
    buildSettings();
}
