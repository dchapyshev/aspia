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
#include <QPointer>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/net/udp_channel.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/android/biometric_gate.h"
#include "client/android/master_password_dialog.h"
#include "common/android/about_widget.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"

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
      about_button_(new IconButton(":/img/material/info.svg", this)),
      desktop_config_(settings_.desktopConfig()),
      udp_methods_(settings_.udpMethods())
{
    // The about action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    about_button_->hide();
    connect(about_button_, &IconButton::clicked, this, &SettingsWidget::showAbout);

    buildSettings();

    stack_->addWidget(settings_page_);
    stack_->addWidget(about_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(stack_);
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
void SettingsWidget::retranslate()
{
    // The controls are built with translated strings, so the content is rebuilt. The current
    // values come from the in-memory state, so nothing is lost.
    buildSettings();
    about_page_->retranslate();
}

//--------------------------------------------------------------------------------------------------
QList<QWidget*> SettingsWidget::appBarActions() const
{
    if (isAboutPage())
        return {};
    return { about_button_ };
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::goBack()
{
    if (!isAboutPage())
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
bool SettingsWidget::isAboutPage() const
{
    return stack_->currentWidget() == about_page_;
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
    buildUdpSection(layout);
    buildDesktopSection(layout);
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

    ComboBox* language = new ComboBox();
    language->setLabel(tr("Language"));
    for (const auto& locale : GuiApplication::instance()->localeList())
        language->addItem(locale.second, locale.first);
    language->setCurrentIndex(qMax(0, language->findData(settings_.locale())));
    connect(language, &QComboBox::currentIndexChanged, this, [this, language](int /* index */)
    {
        const QString locale = language->currentData().toString();
        settings_.setLocale(locale);
        GuiApplication::instance()->setLocale(locale);
    });
    layout->addWidget(language);

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
    connect(change_password, &Button::clicked, this, [this]()
    {
        MasterPasswordDialog dialog(MasterPasswordDialog::Mode::CHANGE, this);
        dialog.exec();
    });
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
void SettingsWidget::buildUdpSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("UDP Connections"));

    const auto add_flag = [this, layout](const QString& text, quint32 flag)
    {
        addBoolSetting(layout, text, (udp_methods_ & flag) != 0, [this, flag](bool checked)
        {
            if (checked)
                udp_methods_ |= flag;
            else
                udp_methods_ &= ~flag;
            settings_.setUdpMethods(udp_methods_);
        });
    };

    add_flag(tr("Allow direct connections"), UDP_METHOD_DIRECT);
    add_flag(tr("Allow UDP Hole Punching"), UDP_METHOD_HOLE_PUNCHING);
    add_flag(tr("Allow PCP protocol"), UDP_METHOD_PCP);
    add_flag(tr("Allow NAT-PMP protocol"), UDP_METHOD_NAT_PMP);
    add_flag(tr("Allow UPnP protocol"), UDP_METHOD_UPNP);
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildDesktopSection(QVBoxLayout* layout)
{
    addSectionHeader(layout, tr("Remote Desktop"));

    addBoolSetting(layout, tr("Enable audio"), desktop_config_.audio(), [this](bool checked)
    {
        desktop_config_.set_audio(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Enable clipboard"), desktop_config_.clipboard(), [this](bool checked)
    {
        desktop_config_.set_clipboard(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Show shape of remote cursor"), desktop_config_.cursor_shape(),
                   [this](bool checked)
    {
        desktop_config_.set_cursor_shape(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Show position of remote cursor"), desktop_config_.cursor_position(),
                   [this](bool checked)
    {
        desktop_config_.set_cursor_position(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Disable desktop effects"), !desktop_config_.effects(),
                   [this](bool checked)
    {
        desktop_config_.set_effects(!checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Disable desktop wallpaper"), !desktop_config_.wallpaper(),
                   [this](bool checked)
    {
        desktop_config_.set_wallpaper(!checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Lock computer at disconnect"), desktop_config_.lock_at_disconnect(),
                   [this](bool checked)
    {
        desktop_config_.set_lock_at_disconnect(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
    addBoolSetting(layout, tr("Block remote input"), desktop_config_.block_input(),
                   [this](bool checked)
    {
        desktop_config_.set_block_input(checked);
        settings_.setDesktopConfig(desktop_config_);
    });
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
    MasterPasswordDialog dialog(MasterPasswordDialog::Mode::UNLOCK, this);
    if (dialog.exec() != QDialog::Accepted)
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
