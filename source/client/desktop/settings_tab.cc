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

#include "client/desktop/settings_tab.h"

#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/udp_channel.h"
#include "build/build_config.h"
#include "client/application.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/settings.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/update_dialog.h"
#include "proto/desktop_control.h"
#include "ui_settings_tab.h"

namespace {

// Most common screen resolutions offered in addition to the client's own monitors.
const QSize kPredefinedResolutions[] =
{
    { 3840, 2160 }, { 2560, 1440 }, { 1920, 1200 }, { 1920, 1080 }, { 1680, 1050 },
    { 1600, 900 }, { 1440, 900 }, { 1366, 768 }, { 1280, 1024 }, { 1280, 800 },
    { 1280, 720 }, { 1024, 768 }, { 800, 600 },
};

//--------------------------------------------------------------------------------------------------
QList<QSize> availableResolutions()
{
    QList<QSize> result;

    auto add = [&](const QSize& size)
    {
        if (size.width() > 0 && size.height() > 0 && !result.contains(size))
            result.append(size);
    };

    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : std::as_const(screens))
    {
        qreal dpr = screen->devicePixelRatio();
        add(QSize(qRound(screen->size().width() * dpr), qRound(screen->size().height() * dpr)));
    }

    for (const QSize& resolution : kPredefinedResolutions)
        add(resolution);

    std::sort(result.begin(), result.end(), [](const QSize& a, const QSize& b)
    {
        return a.width() * a.height() > b.width() * b.height();
    });

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
SettingsTab::SettingsTab(QWidget* parent)
    : Tab(Type::SETTINGS, "settings", parent),
      ui(std::make_unique<Ui::SettingsTab>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    Settings settings;
    Database& db = Database::instance();

    category_group_ = new QButtonGroup(this);
    category_group_->setExclusive(true);

    applyCategoryStyle();

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(ui->category_panel->layout());

    auto add_button = [this, layout](int id, const QString& icon, const QString& text)
    {
        QToolButton* button = new QToolButton(ui->category_panel);
        button->setIcon(QIcon(icon));
        button->setText(text);
        button->setIconSize(QSize(32, 32));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFixedSize(96, 64);
        category_group_->addButton(button, id);
        // Insert before the trailing spacer (last item in layout).
        layout->insertWidget(layout->count() - 1, button, 0, Qt::AlignHCenter);
    };

    int button_id = 0;
    add_button(button_id++, ":/img/gear.svg", tr("General"));
    add_button(button_id++, ":/img/workstation.svg", tr("Desktop"));
#if defined(Q_OS_WINDOWS)
    add_button(button_id++, ":/img/restart.svg", tr("Update"));
#endif

    // Limit content width and center horizontally.
    constexpr int kMaxContentWidth = 960;
    auto center = [](QHBoxLayout* outer, QWidget* content, int max_w)
    {
        content->setMaximumWidth(max_w);
        QSizePolicy policy = content->sizePolicy();
        policy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
        content->setSizePolicy(policy);
        // Re-add content with high stretch and sandwich it between spacers with low stretch - content
        // fills almost all width up to max_w, and only after it hits the cap do the spacers start to grow.
        outer->removeWidget(content);
        outer->insertStretch(0, 1);
        outer->addWidget(content, 2);
        outer->addStretch(1);
    };
    center(ui->layout_general_outer, ui->page_general_content, kMaxContentWidth);
    center(ui->layout_desktop_outer, ui->page_desktop_content, kMaxContentWidth);
    center(ui->layout_update_outer, ui->page_update_content, kMaxContentWidth);

    // General page.
    QString current_locale = settings.locale();
    Application::LocaleList locale_list = GuiApplication::instance()->localeList();
    for (const auto& locale : std::as_const(locale_list))
    {
        ui->combo_language->addItem(locale.second, locale.first);
        if (locale.first == current_locale)
            ui->combo_language->setCurrentIndex(ui->combo_language->count() - 1);
    }

    QString current_theme = settings.theme();
    QStringList available_themes = GuiApplication::instance()->availableThemes();
    for (const QString& theme_id : std::as_const(available_themes))
    {
        ui->combo_theme->addItem(GuiApplication::themeName(theme_id), theme_id);
        if (theme_id == current_theme)
            ui->combo_theme->setCurrentIndex(ui->combo_theme->count() - 1);
    }

    ui->edit_display_name->setText(db.displayName());

    const quint32 udp_methods = settings.udpMethods();
    ui->checkbox_udp_direct->setChecked(udp_methods & UDP_METHOD_DIRECT);
    ui->checkbox_udp_hole_punching->setChecked(udp_methods & UDP_METHOD_HOLE_PUNCHING);
    ui->checkbox_udp_pcp->setChecked(udp_methods & UDP_METHOD_PCP);
    ui->checkbox_udp_natpmp->setChecked(udp_methods & UDP_METHOD_NAT_PMP);
    ui->checkbox_udp_upnp->setChecked(udp_methods & UDP_METHOD_UPNP);

    // Desktop page.
    proto::control::Config desktop_config = settings.desktopConfig();
    ui->checkbox_audio->setChecked(desktop_config.audio());
    ui->checkbox_clipboard->setChecked(desktop_config.clipboard());
    ui->checkbox_cursor_shape->setChecked(desktop_config.cursor_shape());
    ui->checkbox_enable_cursor_pos->setChecked(desktop_config.cursor_position());
    ui->checkbox_desktop_effects->setChecked(!desktop_config.effects());
    ui->checkbox_desktop_wallpaper->setChecked(!desktop_config.wallpaper());
    ui->checkbox_lock_at_disconnect->setChecked(desktop_config.lock_at_disconnect());
    ui->checkbox_block_remote_input->setChecked(desktop_config.block_input());
    ui->checkbox_send_key_combinations->setChecked(settings.sendKeyCombinations());

    ui->combo_resolution->addItem(tr("None"), QSize());
    const QList<QSize> resolutions = availableResolutions();
    for (const QSize& resolution : std::as_const(resolutions))
        ui->combo_resolution->addItem(QString("%1x%2").arg(resolution.width()).arg(resolution.height()), resolution);

    QSize preferred(desktop_config.preferred_resolution().width(),
                    desktop_config.preferred_resolution().height());
    if (!preferred.isEmpty())
    {
        int index = ui->combo_resolution->findData(preferred);
        if (index < 0)
        {
            ui->combo_resolution->addItem(
                QString("%1x%2").arg(preferred.width()).arg(preferred.height()), preferred);
            index = ui->combo_resolution->count() - 1;
        }
        ui->combo_resolution->setCurrentIndex(index);
    }

    ui->checkbox_record_autostart->setChecked(settings.recordSessions());
    ui->edit_record_dir->setText(settings.recordingPath());

    // Update page.
#if defined(Q_OS_WINDOWS)
    ui->checkbox_check_updates->setChecked(db.isCheckUpdatesEnabled());

    QString update_server = db.updateServer();
    ui->edit_update_server->setText(update_server);

    if (update_server == DEFAULT_UPDATE_SERVER)
    {
        ui->checkbox_custom_server->setChecked(false);
        ui->edit_update_server->setEnabled(false);
    }
    else
    {
        ui->checkbox_custom_server->setChecked(true);
        ui->edit_update_server->setEnabled(true);
    }
#endif

    // Wire signals after initial values are loaded to avoid spurious saves.
    connect(category_group_, &QButtonGroup::idClicked, this, &SettingsTab::onCategoryChanged);

    connect(ui->combo_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onLanguageChanged);
    connect(ui->combo_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onThemeChanged);
    connect(ui->edit_display_name, &QLineEdit::editingFinished,
            this, &SettingsTab::onDisplayNameChanged);
    connect(ui->checkbox_udp_direct, &QCheckBox::toggled, this, &SettingsTab::onUdpMethodsChanged);
    connect(ui->checkbox_udp_hole_punching, &QCheckBox::toggled, this, &SettingsTab::onUdpMethodsChanged);
    connect(ui->checkbox_udp_pcp, &QCheckBox::toggled, this, &SettingsTab::onUdpMethodsChanged);
    connect(ui->checkbox_udp_natpmp, &QCheckBox::toggled, this, &SettingsTab::onUdpMethodsChanged);
    connect(ui->checkbox_udp_upnp, &QCheckBox::toggled, this, &SettingsTab::onUdpMethodsChanged);
    connect(ui->button_change_master_password, &QPushButton::clicked,
            this, &SettingsTab::onChangeMasterPassword);

    connect(ui->checkbox_audio, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_clipboard, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_cursor_shape, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_enable_cursor_pos, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_desktop_effects, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_desktop_wallpaper, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_lock_at_disconnect, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_block_remote_input, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_send_key_combinations, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->combo_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onDesktopFeatureChanged);

    connect(ui->checkbox_record_autostart, &QCheckBox::toggled, this, &SettingsTab::onRecordAutostartChanged);
    connect(ui->edit_record_dir, &QLineEdit::editingFinished, this, &SettingsTab::onRecordingPathChanged);
    connect(ui->button_select_record_dir, &QPushButton::clicked, this, &SettingsTab::onSelectRecordingPath);

#if defined(Q_OS_WINDOWS)
    connect(ui->checkbox_check_updates, &QCheckBox::toggled, this, &SettingsTab::onCheckUpdatesChanged);
    connect(ui->checkbox_custom_server, &QCheckBox::toggled, this, &SettingsTab::onCustomServerToggled);
    connect(ui->edit_update_server, &QLineEdit::editingFinished, this, &SettingsTab::onUpdateServerChanged);
    connect(ui->button_check_for_updates, &QPushButton::clicked, this, &SettingsTab::onCheckForUpdatesClicked);
#endif

    if (QAbstractButton* first = category_group_->button(0))
        first->setChecked(true);
    onCategoryChanged(0);
}

//--------------------------------------------------------------------------------------------------
SettingsTab::~SettingsTab()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QByteArray SettingsTab::saveState()
{
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::restoreState(const QByteArray& /* state */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::activate(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::deactivate(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SettingsTab::hasStatusBar() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange ||
        event->type() == QEvent::ApplicationPaletteChange)
    {
        applyCategoryStyle();
    }
    Tab::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCategoryChanged(int id)
{
    if (id >= 0 && id < ui->page_stack->count())
        ui->page_stack->setCurrentIndex(id);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onLanguageChanged()
{
    QString new_locale = ui->combo_language->currentData().toString();
    LOG(INFO) << "[ACTION] Language changed:" << new_locale;

    // The language is not switched live; the new value is applied on the next application start.
    Settings settings;
    settings.setLocale(new_locale);

    MsgBox::information(this,
        tr("The new language will be applied after the application is restarted."));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onThemeChanged()
{
    QString new_theme = ui->combo_theme->currentData().toString();
    LOG(INFO) << "[ACTION] Theme changed:" << new_theme;

    Settings settings;
    settings.setTheme(new_theme);
    GuiApplication::instance()->setTheme(new_theme);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onDisplayNameChanged()
{
    LOG(INFO) << "[ACTION] Display name changed";
    Database::instance().setDisplayName(ui->edit_display_name->text());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onUdpMethodsChanged()
{
    LOG(INFO) << "[ACTION] UDP methods changed";

    quint32 methods = 0;
    if (ui->checkbox_udp_direct->isChecked())
        methods |= UDP_METHOD_DIRECT;
    if (ui->checkbox_udp_hole_punching->isChecked())
        methods |= UDP_METHOD_HOLE_PUNCHING;
    if (ui->checkbox_udp_pcp->isChecked())
        methods |= UDP_METHOD_PCP;
    if (ui->checkbox_udp_natpmp->isChecked())
        methods |= UDP_METHOD_NAT_PMP;
    if (ui->checkbox_udp_upnp->isChecked())
        methods |= UDP_METHOD_UPNP;

    Settings().setUdpMethods(methods);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onDesktopFeatureChanged()
{
    LOG(INFO) << "[ACTION] Desktop feature changed";
    saveDesktopConfig();
    emit sig_desktopConfigChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRecordAutostartChanged()
{
    LOG(INFO) << "[ACTION] Record autostart changed";
    Settings().setRecordSessions(ui->checkbox_record_autostart->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRecordingPathChanged()
{
    LOG(INFO) << "[ACTION] Recording path changed";
    Settings().setRecordingPath(ui->edit_record_dir->text());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onSelectRecordingPath()
{
    LOG(INFO) << "[ACTION] Select recording path";

    QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose path"), ui->edit_record_dir->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Recording path selection rejected";
        return;
    }

    LOG(INFO) << "[ACTION] Recording path selected:" << path;
    ui->edit_record_dir->setText(path);
    Settings().setRecordingPath(path);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onChangeMasterPassword()
{
    LOG(INFO) << "[ACTION] Change master password";

    CredentialsDialog dialog(CredentialsDialog::Type::CHANGE_PASSWORD, this);
    dialog.setWindowTitle(tr("Change Master Password"));
    dialog.setHeaderIcon(":/img/lock.svg");
    dialog.setHeaderText(tr("Enter your current password and choose a new one."));
    dialog.setValidator([this](CredentialsDialog* d) -> bool
    {
        SecureString current = d->currentPassword();
        SecureString new_password = d->password();

        if (!MasterPassword::isSafePassword(new_password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                              "numbers and should not be shorter than %n characters.",
                              "", MasterPassword::kSafePasswordLength);
            QString question = tr("Do you want to enter a different password?");

            if (MsgBox::warning(d, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                return false;
        }

        if (!MasterPassword::change(current, new_password))
        {
            MsgBox::warning(d, tr("Invalid current password or unable to change it."));
            return false;
        }
        return true;
    });
    dialog.exec();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCheckUpdatesChanged()
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "[ACTION] Check updates changed";
    Database::instance().setCheckUpdatesEnabled(ui->checkbox_check_updates->isChecked());
#endif
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCustomServerToggled(bool checked)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "[ACTION] Custom server checkbox:" << checked;
    ui->edit_update_server->setEnabled(checked);

    if (!checked)
    {
        QSignalBlocker blocker(ui->edit_update_server);
        ui->edit_update_server->setText(DEFAULT_UPDATE_SERVER);
        Database::instance().setUpdateServer(QString(DEFAULT_UPDATE_SERVER).toLower());
    }
#endif
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onUpdateServerChanged()
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "[ACTION] Update server changed";
    Database::instance().setUpdateServer(ui->edit_update_server->text().toLower());
#endif
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCheckForUpdatesClicked()
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "[ACTION] Check for updates";
    UpdateDialog(ui->edit_update_server->text(), "client", this).exec();
#endif
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::applyCategoryStyle()
{
    const QPalette& plt = palette();
    QColor checked = plt.color(QPalette::Highlight);
    QColor hover = plt.color(QPalette::Midlight);
    const QColor& checked_text = plt.color(QPalette::Text);

    // The full Highlight color is too dark for category buttons on a light background.
    // Tint it to ~25% opacity so the selection is visible but soft.
    checked.setAlpha(64);
    hover.setAlpha(128);

    ui->category_panel->setStyleSheet(QString(
        "QToolButton {"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "}"
        "QToolButton:hover { background-color: %1; }"
        "QToolButton:checked { background-color: %2; color: %3; }"
    ).arg(hover.name(QColor::HexArgb), checked.name(QColor::HexArgb), checked_text.name(QColor::HexArgb)));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::saveDesktopConfig()
{
    proto::control::Config desktop_config;
    desktop_config.set_audio(ui->checkbox_audio->isChecked());
    desktop_config.set_clipboard(ui->checkbox_clipboard->isChecked());
    desktop_config.set_cursor_shape(ui->checkbox_cursor_shape->isChecked());
    desktop_config.set_cursor_position(ui->checkbox_enable_cursor_pos->isChecked());
    desktop_config.set_effects(!ui->checkbox_desktop_effects->isChecked());
    desktop_config.set_wallpaper(!ui->checkbox_desktop_wallpaper->isChecked());
    desktop_config.set_lock_at_disconnect(ui->checkbox_lock_at_disconnect->isChecked());
    desktop_config.set_block_input(ui->checkbox_block_remote_input->isChecked());

    QSize preferred = ui->combo_resolution->currentData().toSize();
    if (preferred.isValid())
    {
        proto::control::Size* resolution = desktop_config.mutable_preferred_resolution();
        resolution->set_width(preferred.width());
        resolution->set_height(preferred.height());
    }

    Settings settings;
    settings.setDesktopConfig(desktop_config);
    settings.setSendKeyCombinations(ui->checkbox_send_key_combinations->isChecked());
}
