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

#include <QSize>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/net/udp_channel.h"
#include "client/android/master_password_dialog.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/label.h"
#include "common/android/switch.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;
constexpr int kSectionSpacing = 24;

// Predefined resolutions offered in addition to "None", largest first.
const QSize kResolutions[] =
{
    QSize(3840, 2160), QSize(2560, 1440), QSize(1920, 1200), QSize(1920, 1080),
    QSize(1680, 1050), QSize(1600, 900),  QSize(1440, 900),  QSize(1366, 768),
    QSize(1280, 1024), QSize(1280, 800),  QSize(1280, 720),  QSize(1024, 768),
    QSize(800, 600)
};

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent)
    : ScrollArea(parent),
      desktop_config_(settings_.desktopConfig()),
      udp_methods_(settings_.udpMethods())
{
    buildContent();
}

//--------------------------------------------------------------------------------------------------
SettingsWidget::~SettingsWidget() = default;

//--------------------------------------------------------------------------------------------------
void SettingsWidget::retranslate()
{
    // The controls are built with translated strings, so the content is rebuilt. The current
    // values come from the in-memory state, so nothing is lost.
    buildContent();
}

//--------------------------------------------------------------------------------------------------
void SettingsWidget::buildContent()
{
    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);

    buildInterfaceSection(layout);
    buildSecuritySection(layout);
    buildUdpSection(layout);
    buildDesktopSection(layout);
    layout->addStretch();

    // setWidget() deletes the previously set content widget.
    setWidget(content);
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
    addBoolSetting(layout, tr("Send key combinations"), settings_.sendKeyCombinations(),
                   [this](bool checked)
    {
        settings_.setSendKeyCombinations(checked);
    });

    ComboBox* resolution = new ComboBox();
    resolution->setLabel(tr("Preferred resolution"));
    resolution->addItem(tr("None"), QSize());
    for (const QSize& size : kResolutions)
        resolution->addItem(QString("%1x%2").arg(size.width()).arg(size.height()), size);

    const QSize current(desktop_config_.preferred_resolution().width(),
                        desktop_config_.preferred_resolution().height());
    resolution->setCurrentIndex(qMax(0, resolution->findData(current)));
    connect(resolution, &QComboBox::currentIndexChanged, this, [this, resolution](int /* index */)
    {
        const QSize size = resolution->currentData().toSize();
        if (size.isEmpty())
        {
            desktop_config_.clear_preferred_resolution();
        }
        else
        {
            desktop_config_.mutable_preferred_resolution()->set_width(size.width());
            desktop_config_.mutable_preferred_resolution()->set_height(size.height());
        }
        settings_.setDesktopConfig(desktop_config_);
    });
    layout->addWidget(resolution);
}
