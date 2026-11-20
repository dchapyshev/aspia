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

#include "client/android/profile_editor.h"

#include <QVBoxLayout>

#include "base/logging.h"
#include "base/net/udp_channel.h"
#include "client/config.h"
#include "client/router/router_controller.h"
#include "client/storage/database.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;
constexpr int kSectionSpacing = 24;

} // namespace

//--------------------------------------------------------------------------------------------------
ProfileEditor::ProfileEditor(QWidget* parent)
    : QWidget(parent),
      edit_name_(new LineEdit()),
      switch_audio_(new Switch(tr("Enable audio"))),
      switch_clipboard_(new Switch(tr("Enable clipboard"))),
      switch_cursor_shape_(new Switch(tr("Show shape of remote cursor"))),
      switch_cursor_position_(new Switch(tr("Show position of remote cursor"))),
      switch_effects_(new Switch(tr("Disable desktop effects"))),
      switch_wallpaper_(new Switch(tr("Disable desktop wallpaper"))),
      switch_lock_at_disconnect_(new Switch(tr("Lock computer at disconnect"))),
      switch_block_input_(new Switch(tr("Block remote input"))),
      combo_quality_(new ComboBox()),
      switch_udp_direct_(new Switch(tr("Allow direct connections"))),
      switch_udp_hole_punching_(new Switch(tr("Allow UDP Hole Punching"))),
      switch_udp_pcp_(new Switch(tr("Allow PCP protocol"))),
      switch_udp_natpmp_(new Switch(tr("Allow NAT-PMP protocol"))),
      switch_udp_upnp_(new Switch(tr("Allow UPnP protocol"))),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_name_->setLabel(tr("Name"));
    edit_name_->setMaxLength(ProfileConfig::kMaxNameLength);

    combo_quality_->setLabel(tr("Quality"));
    combo_quality_->addItem(tr("Automatic"), kAutoQuality);
    combo_quality_->addItem(tr("Low"), kLowQuality);
    combo_quality_->addItem(tr("Medium"), kMediumQuality);
    combo_quality_->addItem(tr("High"), kHighQuality);

    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    button_delete_ = new Button(tr("Delete"), Button::Role::TEXT);
    button_delete_->setAccentColor(Controls::errorColor());
    button_delete_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_name_);

    form_layout->addSpacing(kSectionSpacing);
    form_layout->addWidget(new Label(tr("Remote Desktop"), Label::Role::CAPTION));
    form_layout->addWidget(switch_audio_);
    form_layout->addWidget(switch_clipboard_);
    form_layout->addWidget(switch_cursor_shape_);
    form_layout->addWidget(switch_cursor_position_);
    form_layout->addWidget(switch_effects_);
    form_layout->addWidget(switch_wallpaper_);
    form_layout->addWidget(switch_lock_at_disconnect_);
    form_layout->addWidget(switch_block_input_);
    form_layout->addWidget(combo_quality_);

    form_layout->addSpacing(kSectionSpacing);
    form_layout->addWidget(new Label(tr("UDP Connections"), Label::Role::CAPTION));
    form_layout->addWidget(switch_udp_direct_);
    form_layout->addWidget(switch_udp_hole_punching_);
    form_layout->addWidget(switch_udp_pcp_);
    form_layout->addWidget(switch_udp_natpmp_);
    form_layout->addWidget(switch_udp_upnp_);

    form_layout->addSpacing(kSectionSpacing);
    form_layout->addWidget(save);
    form_layout->addWidget(button_delete_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &ProfileEditor::onSaveClicked);
    connect(button_delete_, &Button::clicked, this, &ProfileEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
ProfileEditor::~ProfileEditor() = default;

//--------------------------------------------------------------------------------------------------
void ProfileEditor::prepareForAdd()
{
    Database& db = Database::instance();

    const ProfileConfig default_profile = db.defaultProfile();

    profile_id_ = -1;
    desktop_config_ = default_profile.desktopConfig();

    edit_name_->clear();
    edit_name_->show();
    label_error_->setVisible(false);
    button_delete_->hide();

    showParameters(desktop_config_, default_profile.udpMethods());

    edit_name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool ProfileEditor::prepareForEdit(qint64 profile_id)
{
    Database& db = Database::instance();

    if (profile_id <= 0)
    {
        const ProfileConfig default_profile = db.defaultProfile();

        profile_id_ = 0;
        desktop_config_ = default_profile.desktopConfig();

        edit_name_->clear();
        edit_name_->hide();
        label_error_->setVisible(false);
        button_delete_->hide();

        showParameters(desktop_config_, default_profile.udpMethods());
        return true;
    }

    ProfileConfig profile;
    const Database::FindResult found = db.findProfile(profile_id, &profile);
    if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Profile not found:" << profile_id;
        return false;
    }

    profile_id_ = profile_id;

    edit_name_->setText(profile.name());
    edit_name_->show();
    label_error_->setVisible(false);
    button_delete_->show();

    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of profile" << profile_id << "could not be read";

        desktop_config_ = defaultDesktopConfig();
        showParameters(desktop_config_, UDP_METHOD_ALL);
        showError(tr("The profile is damaged. Fix or delete it."));
        return true;
    }

    desktop_config_ = profile.desktopConfig();
    showParameters(desktop_config_, profile.udpMethods());
    return true;
}

//--------------------------------------------------------------------------------------------------
void ProfileEditor::onSaveClicked()
{
    Database& db = Database::instance();

    ProfileConfig profile;
    profile.setId(profile_id_);

    if (profile_id_ != 0)
    {
        const QString name = edit_name_->text().trimmed();
        if (name.isEmpty())
        {
            showError(tr("Name cannot be empty."));
            edit_name_->setFocus();
            return;
        }

        if (name.length() > ProfileConfig::kMaxNameLength)
        {
            showError(tr("Too long name. The maximum length of the name is %n characters.",
                         "", ProfileConfig::kMaxNameLength));
            edit_name_->setFocus();
            edit_name_->selectAll();
            return;
        }

        QList<ProfileConfig> profiles;
        db.profileList(&profiles);

        bool exists = name.compare(tr("Default"), Qt::CaseInsensitive) == 0;
        for (const ProfileConfig& other : std::as_const(profiles))
        {
            if (other.id() != profile_id_ && other.name().compare(name, Qt::CaseInsensitive) == 0)
            {
                exists = true;
                break;
            }
        }

        if (exists)
        {
            showError(tr("A profile with this name already exists."));
            edit_name_->setFocus();
            edit_name_->selectAll();
            return;
        }

        profile.setName(name);
    }

    desktop_config_.set_audio(switch_audio_->isChecked());
    desktop_config_.set_clipboard(switch_clipboard_->isChecked());
    desktop_config_.set_cursor_shape(switch_cursor_shape_->isChecked());
    desktop_config_.set_cursor_position(switch_cursor_position_->isChecked());
    desktop_config_.set_effects(!switch_effects_->isChecked());
    desktop_config_.set_wallpaper(!switch_wallpaper_->isChecked());
    desktop_config_.set_lock_at_disconnect(switch_lock_at_disconnect_->isChecked());
    desktop_config_.set_block_input(switch_block_input_->isChecked());
    desktop_config_.set_quality(combo_quality_->currentData().toUInt());

    quint32 udp_methods = 0;
    if (switch_udp_direct_->isChecked())
        udp_methods |= UDP_METHOD_DIRECT;
    if (switch_udp_hole_punching_->isChecked())
        udp_methods |= UDP_METHOD_HOLE_PUNCHING;
    if (switch_udp_pcp_->isChecked())
        udp_methods |= UDP_METHOD_PCP;
    if (switch_udp_natpmp_->isChecked())
        udp_methods |= UDP_METHOD_NAT_PMP;
    if (switch_udp_upnp_->isChecked())
        udp_methods |= UDP_METHOD_UPNP;

    profile.setDesktopConfig(desktop_config_);
    profile.setUdpMethods(udp_methods);

    bool saved = false;
    if (profile_id_ == 0)
    {
        ProfileConfig default_profile = db.defaultProfile();
        default_profile.setDesktopConfig(desktop_config_);
        default_profile.setUdpMethods(udp_methods);
        saved = db.modifyProfile(default_profile);
    }
    else if (profile_id_ < 0)
        saved = db.addProfile(profile);
    else
        saved = db.modifyProfile(profile);

    if (!saved)
    {
        showError(tr("Failed to save the profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void ProfileEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Profile"),
                                tr("Delete the profile \"%1\"?").arg(edit_name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeProfile(profile_id_))
    {
        showError(tr("Failed to delete the profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void ProfileEditor::showParameters(const proto::control::Config& desktop_config, quint32 udp_methods)
{
    switch_audio_->setChecked(desktop_config.audio());
    switch_clipboard_->setChecked(desktop_config.clipboard());
    switch_cursor_shape_->setChecked(desktop_config.cursor_shape());
    switch_cursor_position_->setChecked(desktop_config.cursor_position());
    switch_effects_->setChecked(!desktop_config.effects());
    switch_wallpaper_->setChecked(!desktop_config.wallpaper());
    switch_lock_at_disconnect_->setChecked(desktop_config.lock_at_disconnect());
    switch_block_input_->setChecked(desktop_config.block_input());
    combo_quality_->setCurrentIndex(qMax(0, combo_quality_->findData(desktop_config.quality())));

    switch_udp_direct_->setChecked(udp_methods & UDP_METHOD_DIRECT);
    switch_udp_hole_punching_->setChecked(udp_methods & UDP_METHOD_HOLE_PUNCHING);
    switch_udp_pcp_->setChecked(udp_methods & UDP_METHOD_PCP);
    switch_udp_natpmp_->setChecked(udp_methods & UDP_METHOD_NAT_PMP);
    switch_udp_upnp_->setChecked(udp_methods & UDP_METHOD_UPNP);
}

//--------------------------------------------------------------------------------------------------
void ProfileEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
