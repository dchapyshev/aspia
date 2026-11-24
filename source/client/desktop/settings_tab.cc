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
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "base/auto_qpointer.h"
#include "base/build_config.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/net/udp_channel.h"
#include "base/peer/user.h"
#include "client/application.h"
#include "client/mcp/mcp_worker.h"
#include "client/desktop/ui_settings_tab.h"
#include "client/desktop/update_server_dialog.h"
#include "client/router/router_controller.h"
#include "client/storage/master_password.h"
#include "client/storage/profile_store.h"
#include "client/storage/router_store.h"
#include "client/storage/settings_store.h"
#include "client/storage/system_settings.h"
#include "client/storage/user_settings.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/elevate_util.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/session_type.h"
#include "common/desktop/update_dialog.h"
#include "proto/desktop_control.h"
#include "proto/peer.h"
#include "proto/router.h"

namespace {

const int kNoProxy = -1;

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

    UserSettings settings;
    SettingsStore store;

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
        button->setFixedSize(120, 64);
        category_group_->addButton(button, id);
        // Insert before the trailing spacer (last item in layout).
        layout->insertWidget(layout->count() - 1, button, 0, Qt::AlignHCenter);
    };

    int button_id = 0;
    add_button(button_id++, ":/img/gear.svg", tr("General"));
    add_button(button_id++, ":/img/dashboard-layout.svg", tr("Profiles"));
    add_button(button_id++, ":/img/ai.svg", "MCP");
    add_button(button_id++, ":/img/restart.svg", tr("Update"));

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
    center(ui->layout_general_footer_outer, ui->page_general_footer_content, kMaxContentWidth);
    center(ui->layout_desktop_outer, ui->page_desktop_content, kMaxContentWidth);
    center(ui->layout_mcp_outer, ui->page_mcp_content, kMaxContentWidth);
    center(ui->layout_update_outer, ui->page_update_content, kMaxContentWidth);

    // A scroll area fills the page with the window color, the pane of the tab has the base one.
    for (QScrollArea* page : { ui->scroll_general, ui->page_desktop, ui->page_mcp, ui->page_update })
    {
        page->viewport()->setAutoFillBackground(false);
        page->widget()->setAutoFillBackground(false);
    }

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

    ui->edit_display_name->setText(store.displayName());
    ui->checkbox_confirm_close->setChecked(settings.isConfirmCloseEnabled());

    ui->checkbox_backup_on_startup->setChecked(store.isBackupOnStartupEnabled());
    ui->edit_backup_dir->setText(store.backupPath());

    ui->combo_backup_retention->addItem(tr("1 week"), static_cast<int>(AutoBackup::Retention::ONE_WEEK));
    ui->combo_backup_retention->addItem(tr("2 weeks"), static_cast<int>(AutoBackup::Retention::TWO_WEEKS));
    ui->combo_backup_retention->addItem(tr("1 month"), static_cast<int>(AutoBackup::Retention::ONE_MONTH));
    ui->combo_backup_retention->addItem(tr("6 months"), static_cast<int>(AutoBackup::Retention::SIX_MONTHS));
    ui->combo_backup_retention->addItem(tr("1 year"), static_cast<int>(AutoBackup::Retention::ONE_YEAR));
    ui->combo_backup_retention->setCurrentIndex(
        ui->combo_backup_retention->findData(static_cast<int>(store.backupRetention())));

    ui->combo_router_session_type->addItem(tr("Administrator"), proto::router::SESSION_TYPE_ADMIN);
    ui->combo_router_session_type->addItem(tr("Manager"), proto::router::SESSION_TYPE_MANAGER);
    ui->combo_router_session_type->addItem(tr("Operator"), proto::router::SESSION_TYPE_OPERATOR);
    ui->edit_router_password->setShowPasswordButtonVisible(true);

    loadRouter();

    ui->button_save_general->setIcon(QIcon(":/img/done.svg"));
    ui->button_save_general->setIconSize(QSize(24, 24));

    ui->combo_lock_timeout->addItem(tr("Do not lock"), 0);
    ui->combo_lock_timeout->addItem(tr("1 minute"), 1);
    ui->combo_lock_timeout->addItem(tr("5 minutes"), 5);
    ui->combo_lock_timeout->addItem(tr("10 minutes"), 10);
    ui->combo_lock_timeout->addItem(tr("30 minutes"), 30);
    ui->combo_lock_timeout->addItem(tr("1 hour"), 60);

    const int lock_index = ui->combo_lock_timeout->findData(static_cast<int>(store.lockTimeout().count()));
    ui->combo_lock_timeout->setCurrentIndex(lock_index >= 0 ? lock_index : 0);

#if defined(Q_OS_LINUX)
    // There is no keystore of the user here, and the key would be wrapped with the identity of the
    // computer that anyone on it can read.
    ui->checkbox_auto_unlock->hide();
#else
    const bool auto_unlock = MasterPassword::isAutoUnlockEnabled();
    ui->checkbox_auto_unlock->setChecked(auto_unlock);
    ui->combo_lock_timeout->setEnabled(!auto_unlock);
#endif // defined(Q_OS_LINUX)

    ui->combo_proxy_type->addItem(tr("No proxy"), kNoProxy);
    ui->combo_proxy_type->addItem("HTTP", static_cast<int>(ProxySettings::Type::HTTP));
    ui->combo_proxy_type->addItem("HTTPS", static_cast<int>(ProxySettings::Type::HTTPS));
    ui->combo_proxy_type->addItem("SOCKS4", static_cast<int>(ProxySettings::Type::SOCKS4));
    ui->combo_proxy_type->addItem("SOCKS4A", static_cast<int>(ProxySettings::Type::SOCKS4A));
    ui->combo_proxy_type->addItem("SOCKS5", static_cast<int>(ProxySettings::Type::SOCKS5));
    ui->combo_proxy_type->addItem("SOCKS5H", static_cast<int>(ProxySettings::Type::SOCKS5H));

    ProxyConfig proxy;
    store.proxyConfig(&proxy);
    const ProxySettings& proxy_settings = proxy.settings();
    ui->combo_proxy_type->setCurrentIndex(
        ui->combo_proxy_type->findData(proxy_settings.enabled ? static_cast<int>(proxy_settings.type) : kNoProxy));
    ui->edit_proxy_address->setText(proxy_settings.address);
    if (proxy_settings.port)
        ui->spinbox_proxy_port->setValue(proxy_settings.port);
    ui->edit_proxy_username->setText(proxy_settings.user);
    ui->edit_proxy_password->setPassword(proxy_settings.password);
    updateProxyFields();

    // Profiles page.
    ui->combo_resolution->addItem(tr("None"), QSize());
    const QList<QSize> resolutions = availableResolutions();
    for (const QSize& resolution : std::as_const(resolutions))
        ui->combo_resolution->addItem(QString("%1x%2").arg(resolution.width()).arg(resolution.height()), resolution);

    ui->combo_quality->addItem(tr("Automatic"), kAutoQuality);
    ui->combo_quality->addItem(tr("Low"), kLowQuality);
    ui->combo_quality->addItem(tr("Medium"), kMediumQuality);
    ui->combo_quality->addItem(tr("High"), kHighQuality);

    reloadProfiles(0);

    // MCP page.
    ui->checkbox_mcp_enabled->setChecked(settings.isMcpEnabled());
    ui->checkbox_mcp_auto_confirm->setChecked(settings.isMcpAutoConfirmEnabled());
    ui->checkbox_mcp_sign_in->setChecked(settings.isMcpSignInEnabled());

    mcp_session_types_ = new QButtonGroup(this);
    mcp_session_types_->setExclusive(false);

    const quint32 mcp_session_types = settings.mcpSessionTypes();
    for (proto::peer::SessionType session_type : { proto::peer::SESSION_TYPE_DESKTOP,
                                                   proto::peer::SESSION_TYPE_FILE_TRANSFER,
                                                   proto::peer::SESSION_TYPE_SYSTEM_INFO,
                                                   proto::peer::SESSION_TYPE_CHAT,
                                                   proto::peer::SESSION_TYPE_TERMINAL })
    {
        QCheckBox* checkbox = new QCheckBox(sessionName(session_type), ui->groupbox_mcp_session_types);
        checkbox->setChecked(mcp_session_types & session_type);
        ui->layout_mcp_session_types->addWidget(checkbox);
        mcp_session_types_->addButton(checkbox, session_type);
    }

    ui->edit_mcp_command->setText(
        QString("\"%1\" --mcp").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));

    ui->label_mcp_command->setEnabled(ui->checkbox_mcp_enabled->isChecked());
    ui->edit_mcp_command->setEnabled(ui->checkbox_mcp_enabled->isChecked());
    ui->button_copy_mcp_command->setEnabled(ui->checkbox_mcp_enabled->isChecked());
    ui->groupbox_mcp_session_types->setEnabled(ui->checkbox_mcp_enabled->isChecked());
    ui->groupbox_mcp_actions->setEnabled(ui->checkbox_mcp_enabled->isChecked());

    // Update page.
    ui->checkbox_check_updates->setChecked(settings.isCheckUpdatesEnabled());

    ui->combobox_update_channel->addItem(tr("Stable"), kStableUpdateChannel);
    ui->combobox_update_channel->addItem(tr("Beta"), kBetaUpdateChannel);
    ui->combobox_update_channel->addItem(tr("Alpha"), kAlphaUpdateChannel);

    int channel_index = ui->combobox_update_channel->findData(settings.updateChannel());
    ui->combobox_update_channel->setCurrentIndex(channel_index >= 0 ? channel_index : 0);

    loadUpdateServer();

    // Wire signals after initial values are loaded to avoid spurious saves.
    connect(category_group_, &QButtonGroup::idClicked, this, &SettingsTab::onCategoryChanged);

    // Every field of the general page waits for the save button.
    for (QComboBox* combo : { ui->combo_language, ui->combo_theme, ui->combo_backup_retention,
                              ui->combo_lock_timeout })
    {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsTab::onGeneralChanged);
    }

    for (QLineEdit* edit : { ui->edit_display_name, ui->edit_backup_dir, ui->edit_proxy_address,
                             ui->edit_proxy_username, static_cast<QLineEdit*>(ui->edit_proxy_password) })
    {
        connect(edit, &QLineEdit::textChanged, this, &SettingsTab::onGeneralChanged);
    }

    for (QCheckBox* checkbox : { ui->checkbox_confirm_close, ui->checkbox_backup_on_startup })
    {
        connect(checkbox, &QCheckBox::toggled, this, &SettingsTab::onGeneralChanged);
    }

    for (QLineEdit* edit : { ui->edit_router_address, ui->edit_router_username,
                             static_cast<QLineEdit*>(ui->edit_router_password) })
    {
        connect(edit, &QLineEdit::textChanged, this, &SettingsTab::onRouterChanged);
    }

    connect(ui->combo_router_session_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onRouterChanged);
    connect(ui->checkbox_router_enabled, &QCheckBox::toggled, this, &SettingsTab::onRouterEnabledChanged);
    connect(ui->checkbox_router_sync, &QCheckBox::toggled, this, &SettingsTab::onRouterChanged);
    connect(ui->button_router_sync_reset, &QPushButton::clicked, this, &SettingsTab::onResetSyncClicked);
    connect(&RouterController::instance(), &RouterController::sig_statusChanged,
            this, &SettingsTab::onRouterStatusChanged);
    connect(&RouterController::instance(), &RouterController::sig_profilesChanged,
            this, &SettingsTab::onProfilesChanged);

    connect(ui->spinbox_proxy_port, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsTab::onGeneralChanged);
    connect(ui->combo_proxy_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onProxyTypeChanged);
    connect(ui->checkbox_auto_unlock, &QCheckBox::toggled, this, &SettingsTab::onAutoUnlockToggled);
    connect(ui->button_select_backup_dir, &QToolButton::clicked, this, &SettingsTab::onSelectBackupPath);
    connect(ui->button_save_general, &QPushButton::clicked, this, &SettingsTab::onSaveGeneral);
    connect(ui->button_change_master_password, &QPushButton::clicked,
            this, &SettingsTab::onChangeMasterPassword);

    ui->button_save_general->setEnabled(false);

    connect(ui->combo_profile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onProfileChanged);
    connect(ui->button_add_profile, &QToolButton::clicked, this, &SettingsTab::onAddProfile);
    connect(ui->button_rename_profile, &QToolButton::clicked, this, &SettingsTab::onRenameProfile);
    connect(ui->button_delete_profile, &QToolButton::clicked, this, &SettingsTab::onDeleteProfile);

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
    connect(ui->combo_quality, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_preferred_size, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);

    connect(ui->checkbox_record_autostart, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->edit_record_dir, &QLineEdit::editingFinished, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->button_select_record_dir, &QToolButton::clicked, this, &SettingsTab::onSelectRecordingPath);

    connect(ui->checkbox_udp_direct, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_hole_punching, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_pcp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_natpmp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_upnp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);

    connect(ui->checkbox_mcp_enabled, &QCheckBox::toggled, this, &SettingsTab::onMcpEnabledChanged);
    connect(ui->checkbox_mcp_auto_confirm, &QCheckBox::toggled, this, &SettingsTab::onMcpAutoConfirmChanged);
    connect(ui->checkbox_mcp_sign_in, &QCheckBox::toggled, this, &SettingsTab::onMcpSignInChanged);
    connect(mcp_session_types_, &QButtonGroup::idToggled, this, &SettingsTab::onMcpSessionTypesChanged);
    connect(ui->button_copy_mcp_command, &QToolButton::clicked, this, &SettingsTab::onCopyMcpCommand);

    connect(ui->checkbox_check_updates, &QCheckBox::toggled, this, &SettingsTab::onCheckUpdatesChanged);
    connect(ui->combobox_update_channel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onUpdateChannelChanged);
    connect(ui->button_check_for_updates, &QPushButton::clicked, this, &SettingsTab::onCheckForUpdatesClicked);
    connect(ui->button_update_server, &QPushButton::clicked, this, &SettingsTab::onUpdateServerClicked);

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
QVariantMap SettingsTab::saveState()
{
    return QVariantMap();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::restoreState(const QVariantMap& /* state */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::activate()
{
    if (router_changed_)
        return;

    const bool unsaved = ui->button_save_general->isEnabled();
    loadRouter();
    ui->button_save_general->setEnabled(unsaved);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::deactivate()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool SettingsTab::confirmClose()
{
    if (!ui->button_save_general->isEnabled())
        return true;

    if (MsgBox::question(this,
            tr("The settings have unsaved changes. Close the tab without saving?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Closing the settings rejected";
        return false;
    }

    return true;
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
void SettingsTab::onGeneralChanged()
{
    ui->button_save_general->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRouterChanged()
{
    router_changed_ = true;
    onGeneralChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRouterEnabledChanged()
{
    updateRouterFields();
    onRouterChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRouterStatusChanged()
{
    RouterConfig router;
    const Database::FindResult found = RouterStore().find(&router);
    const bool sync_enabled = found == Database::FindResult::FOUND && router.isSyncEnabled();

    if (found == Database::FindResult::FOUND && !router_changed_)
    {
        QSignalBlocker blocker(ui->checkbox_router_sync);
        ui->checkbox_router_sync->setChecked(sync_enabled);
    }

    ui->button_router_sync_reset->setEnabled(sync_enabled && RouterController::status() == RouterStatus::ONLINE);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onResetSyncClicked()
{
    LOG(INFO) << "[ACTION] Reset synchronization";

    const QString question =
        tr("The synchronized data on the router and on the other devices of the account will be replaced with "
           "the data of this device. Reset the synchronization?");

    if (MsgBox::importantQuestion(this, question, Seconds(10)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    if (!RouterController::resetSyncStorage())
        MsgBox::warning(this, tr("Failed to reset the synchronization."));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onSelectBackupPath()
{
    LOG(INFO) << "[ACTION] Select backup path";

    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Choose path"), ui->edit_backup_dir->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (path.isEmpty())
    {
        LOG(INFO) << "[ACTION] Backup path selection rejected";
        return;
    }

    LOG(INFO) << "[ACTION] Backup path selected:" << path;
    ui->edit_backup_dir->setText(path);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onProxyTypeChanged()
{
    updateProxyFields();
    onGeneralChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onAutoUnlockToggled()
{
    ui->combo_lock_timeout->setEnabled(!ui->checkbox_auto_unlock->isChecked());
    onGeneralChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onSaveGeneral()
{
    LOG(INFO) << "[ACTION] Save general settings";

    std::optional<RouterConfig> router;
    if (router_changed_ && !routerFromUi(&router))
        return;

    SettingsStore store;
    UserSettings settings;

    const QString locale = ui->combo_language->currentData().toString();
    const bool locale_changed = locale != settings.locale();
    if (locale_changed)
    {
        LOG(INFO) << "Language changed:" << locale;
        settings.setLocale(locale);
    }

    const QString theme = ui->combo_theme->currentData().toString();
    if (theme != settings.theme())
    {
        LOG(INFO) << "Theme changed:" << theme;
        settings.setTheme(theme);
        GuiApplication::instance()->setTheme(theme);
    }

    settings.setConfirmCloseEnabled(ui->checkbox_confirm_close->isChecked());

    bool written = store.setDisplayName(ui->edit_display_name->text());
    written &= store.setBackupOnStartupEnabled(ui->checkbox_backup_on_startup->isChecked());

    // An empty field names no directory, so the stored one stays.
    if (ui->edit_backup_dir->text().isEmpty())
        ui->edit_backup_dir->setText(store.backupPath());
    else
        written &= store.setBackupPath(ui->edit_backup_dir->text());

    written &= store.setBackupRetention(
        static_cast<AutoBackup::Retention>(ui->combo_backup_retention->currentData().toInt()));

    const int proxy_type = ui->combo_proxy_type->currentData().toInt();

    ProxySettings proxy_settings;
    proxy_settings.enabled = proxy_type != kNoProxy;
    if (proxy_settings.enabled)
        proxy_settings.type = static_cast<ProxySettings::Type>(proxy_type);
    proxy_settings.address = ui->edit_proxy_address->text().trimmed();
    proxy_settings.port = static_cast<quint16>(ui->spinbox_proxy_port->value());
    proxy_settings.user = ui->edit_proxy_username->text();
    proxy_settings.password = ui->edit_proxy_password->password();

    ProxyConfig proxy;
    proxy.setSettings(proxy_settings);
    written &= store.setProxyConfig(proxy);

    // The master password is asked for here, and a refusal leaves the box as the stored state is.
    const bool auto_unlock = ui->checkbox_auto_unlock->isChecked();
    if (auto_unlock != MasterPassword::isAutoUnlockEnabled())
    {
        LOG(INFO) << "Auto unlock changed:" << auto_unlock;

        if (!(auto_unlock ? enableAutoUnlock() : MasterPassword::setAutoUnlockEnabled(false)))
        {
            QSignalBlocker blocker(ui->checkbox_auto_unlock);
            ui->checkbox_auto_unlock->setChecked(MasterPassword::isAutoUnlockEnabled());
            ui->combo_lock_timeout->setEnabled(!ui->checkbox_auto_unlock->isChecked());
        }
    }

    // A locked application would be unlocked by starting it again.
    if (MasterPassword::isAutoUnlockEnabled())
        ui->combo_lock_timeout->setCurrentIndex(ui->combo_lock_timeout->findData(0));

    const Minutes lock_timeout(ui->combo_lock_timeout->currentData().toInt());
    if (lock_timeout != store.lockTimeout())
    {
        written &= store.setLockTimeout(lock_timeout);
        emit sig_lockTimeoutChanged();
    }

    if (!written)
        MsgBox::warning(this, tr("Failed to save the settings."));

    bool router_saved = true;
    if (router.has_value())
    {
        router_saved = saveRouter(*router);
        if (router_saved)
            emit sig_routerChanged();
    }

    if (router_saved)
        loadRouter();

    // What was not written stays in the fields, so the save can be tried again.
    ui->button_save_general->setEnabled(!written || !router_saved);

    if (locale_changed)
        MsgBox::information(this, tr("The new language will be applied after the application is restarted."));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onProfileChanged()
{
    LOG(INFO) << "[ACTION] Profile changed:" << ui->combo_profile->currentData().toLongLong();
    showProfile();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onAddProfile()
{
    LOG(INFO) << "[ACTION] Add profile";

    const QString name = askProfileName(tr("Add Profile"), QString(), -1);
    if (name.isEmpty())
        return;

    // A new profile starts with the parameters of the one shown.
    ProfileConfig profile = profileFromUi();
    profile.setName(name);

    if (!ProfileStore().add(profile))
    {
        MsgBox::warning(this, tr("Unable to add profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    reloadProfiles(profile.id());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRenameProfile()
{
    LOG(INFO) << "[ACTION] Rename profile";

    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();
    if (profile_id <= 0)
        return;

    ProfileStore store;

    ProfileConfig profile;
    if (store.find(profile_id, &profile) != Database::FindResult::FOUND)
    {
        MsgBox::warning(this, tr("Failed to read the profile."));
        return;
    }

    const QString name = askProfileName(tr("Rename Profile"), profile.name(), profile_id);
    if (name.isEmpty())
        return;

    if (store.find(profile_id, &profile) != Database::FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read profile" << profile_id;
        MsgBox::warning(this, tr("Failed to read the profile."));
        return;
    }

    profile.setName(name);

    if (!store.modify(profile))
    {
        MsgBox::warning(this, tr("Unable to rename profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    reloadProfiles(profile_id);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onDeleteProfile()
{
    LOG(INFO) << "[ACTION] Delete profile";

    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();
    if (profile_id <= 0)
        return;

    if (MsgBox::question(this,
            tr("Are you sure you want to delete profile \"%1\"?").arg(ui->combo_profile->currentText()),
            MsgBox::Yes | MsgBox::No) != MsgBox::Yes)
    {
        return;
    }

    if (!ProfileStore().remove(profile_id))
    {
        MsgBox::warning(this, tr("Unable to delete profile."));
        return;
    }

    RouterController::startSyncRecordExchange();
    reloadProfiles(0);
    emit sig_desktopConfigChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onProfilesChanged()
{
    const qint64 shown_profile_id = ui->combo_profile->currentData().toLongLong();

    loadProfileList(shown_profile_id, false);

    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();
    ProfileStore store;
    ProfileConfig profile;

    if (profile_id <= 0)
        profile = store.defaultProfile();
    else if (store.find(profile_id, &profile) != Database::FindResult::FOUND)
        LOG(ERROR) << "Profile" << profile_id << "is not read";

    if (profile_id != shown_profile_id || profile.modifyTime() != profile_modify_time_)
        showProfile(false);

    emit sig_desktopConfigChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onDesktopFeatureChanged()
{
    LOG(INFO) << "[ACTION] Desktop feature changed";
    saveProfile();
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
    onDesktopFeatureChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onMcpEnabledChanged()
{
    const bool enable = ui->checkbox_mcp_enabled->isChecked();
    LOG(INFO) << "[ACTION] MCP enabled changed:" << enable;

    UserSettings().setMcpEnabled(enable);
    ui->label_mcp_command->setEnabled(enable);
    ui->edit_mcp_command->setEnabled(enable);
    ui->button_copy_mcp_command->setEnabled(enable);
    ui->groupbox_mcp_session_types->setEnabled(enable);
    ui->groupbox_mcp_actions->setEnabled(enable);

    if (enable != (GuiApplication::findWorker<McpWorker>() != nullptr))
        MsgBox::information(this, tr("The settings will be applied after the application is restarted."));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onMcpAutoConfirmChanged()
{
    LOG(INFO) << "[ACTION] MCP auto confirm changed";
    UserSettings().setMcpAutoConfirmEnabled(ui->checkbox_mcp_auto_confirm->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onMcpSignInChanged()
{
    LOG(INFO) << "[ACTION] MCP sign in changed";
    UserSettings().setMcpSignInEnabled(ui->checkbox_mcp_sign_in->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onMcpSessionTypesChanged()
{
    LOG(INFO) << "[ACTION] MCP session types changed";

    quint32 session_types = 0;

    const QList<QAbstractButton*> buttons = mcp_session_types_->buttons();
    for (QAbstractButton* button : buttons)
    {
        if (button->isChecked())
            session_types |= static_cast<quint32>(mcp_session_types_->id(button));
    }

    UserSettings().setMcpSessionTypes(session_types);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCopyMcpCommand()
{
    LOG(INFO) << "[ACTION] Copy MCP command";
    QGuiApplication::clipboard()->setText(ui->edit_mcp_command->text());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onChangeMasterPassword()
{
    LOG(INFO) << "[ACTION] Change master password";

    AutoQPointer<CredentialsDialog> dialog(new CredentialsDialog(CredentialsDialog::Type::CHANGE_PASSWORD, this));
    dialog->setWindowTitle(tr("Change Master Password"));
    dialog->setHeaderIcon(":/img/lock.svg");
    dialog->setHeaderText(tr("Enter your current password and choose a new one."));
    dialog->setValidator([this](CredentialsDialog* d) -> bool
    {
        SecureString current = d->currentPassword();
        SecureString new_password = d->password();

        if (new_password.size() < MasterPassword::kMinPasswordLength)
        {
            MsgBox::warning(d, tr("The password can not be shorter than %n characters.",
                                  "", MasterPassword::kMinPasswordLength));
            return false;
        }

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

        switch (MasterPassword::change(current, new_password))
        {
            case MasterPassword::Result::SUCCESS:
                break;

            case MasterPassword::Result::INVALID_PASSWORD:
                MsgBox::warning(d, tr("Invalid current password."));
                return false;

            case MasterPassword::Result::UNREADABLE_RECORD:
                MsgBox::warning(d, tr("Some records of the database are damaged. Fix or delete them and try again."));
                return false;

            default:
                MsgBox::warning(d, tr("Unable to change the password."));
                return false;
        }

        return true;
    });
    dialog->exec();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCheckUpdatesChanged()
{
    LOG(INFO) << "[ACTION] Check updates changed";
    UserSettings().setCheckUpdatesEnabled(ui->checkbox_check_updates->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onUpdateChannelChanged()
{
    QString channel = ui->combobox_update_channel->currentData().toString();
    LOG(INFO) << "[ACTION] Update channel changed:" << channel;
    UserSettings().setUpdateChannel(channel);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCheckForUpdatesClicked()
{
    LOG(INFO) << "[ACTION] Check for updates";

    ProxyConfig proxy;
    if (SettingsStore().proxyConfig(&proxy) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "No readable proxy config";
        MsgBox::warning(this, tr("Unable to read the proxy settings."));
        return;
    }

    SystemSettings settings;
    AutoQPointer<UpdateDialog> dialog(new UpdateDialog(
        settings.updateServer(), settings.updatePublicKey(), ui->combobox_update_channel->currentData().toString(),
        "client", proxy.settings(), QVersionNumber(), UpdateDialog::Action::ASK, this));
    dialog->exec();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onUpdateServerClicked()
{
    LOG(INFO) << "[ACTION] Update server";

    if (ProcessUtil::isPrivileged())
    {
        AutoQPointer<UpdateServerDialog> dialog(new UpdateServerDialog(this));
        dialog->exec();
        loadUpdateServer();
        return;
    }

    // The tab can be closed while the dialog is open, so the process belongs to the main window.
    ElevateUtil* elevate_util = ElevateUtil::create(window()).release();
    if (!elevate_util)
        return;

    ui->button_update_server->setEnabled(false);

    QStringList arguments;
    arguments << "--update-server";
    arguments << "--locale=" + GuiApplication::instance()->locale();

    QPointer<SettingsTab> self(this);
    bool started = elevate_util->runElevated(arguments, window()->winId(),
        [elevate_util, self](int /* exit_code */)
    {
        elevate_util->deleteLater();
        if (!self)
            return;

        self->ui->button_update_server->setEnabled(true);
        self->loadUpdateServer();
    });

    if (!started)
    {
        elevate_util->deleteLater();
        ui->button_update_server->setEnabled(true);
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::loadUpdateServer()
{
    SystemSettings settings;
    QString server = settings.updateServer();
    QString public_key = QString::fromLatin1(settings.updatePublicKey().toHex());

    ui->label_update_server_value->setText(server);
    ui->label_update_public_key_value->setText(public_key);

    ui->label_update_server->setVisible(!server.isEmpty());
    ui->label_update_server_value->setVisible(!server.isEmpty());
    ui->label_update_public_key->setVisible(!server.isEmpty() && !public_key.isEmpty());
    ui->label_update_public_key_value->setVisible(!server.isEmpty() && !public_key.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::updateProxyFields()
{
    const bool enabled = ui->combo_proxy_type->currentData().toInt() != kNoProxy;

    const QList<QWidget*> widgets =
    {
        ui->label_proxy_address, ui->edit_proxy_address, ui->label_proxy_port, ui->spinbox_proxy_port,
        ui->label_proxy_username, ui->edit_proxy_username, ui->label_proxy_password, ui->edit_proxy_password
    };

    for (QWidget* widget : widgets)
        widget->setEnabled(enabled);
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
bool SettingsTab::enableAutoUnlock()
{
#if defined(Q_OS_WINDOWS)
    // On macOS the keychain asks before another application reads the key.
    QString risk = tr("The key of the database will be stored on this computer. Any program running "
                      "under your account will be able to read it, even when the application is not "
                      "running.");
    QString question = tr("Do you want to continue?");

    if (MsgBox::importantQuestion(this, QString("%1\n%2").arg(risk, question), Seconds(10)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Auto unlock warning rejected";
        return false;
    }
#endif // defined(Q_OS_WINDOWS)

    // Otherwise anyone at the unlocked application could make it open without the password.
    AutoQPointer<CredentialsDialog> dialog(new CredentialsDialog(CredentialsDialog::Type::ENTER_PASSWORD, this));
    dialog->setWindowTitle(tr("Unlock Automatically"));
    dialog->setHeaderIcon(":/img/lock.svg");
    dialog->setHeaderText(tr("Enter the master password to unlock the application automatically on startup."));
    dialog->setShowPasswordButtonVisible(true);
    dialog->setValidator([](CredentialsDialog* d) -> bool
    {
        switch (MasterPassword::unlock(d->password()))
        {
            case MasterPassword::Result::SUCCESS:
                return true;

            case MasterPassword::Result::INVALID_PASSWORD:
                MsgBox::warning(d, tr("Invalid master password."));
                return false;

            default:
                MsgBox::warning(d, tr("Unable to unlock the database."));
                return false;
        }
    });

    if (dialog->exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Auto unlock password entry rejected";
        return false;
    }

    if (!MasterPassword::setAutoUnlockEnabled(true))
    {
        LOG(ERROR) << "Unable to turn on auto unlock";
        MsgBox::warning(this, tr("Unable to turn on the automatic unlock."));
        return false;
    }

    LOG(INFO) << "Auto unlock turned on";
    return true;
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::loadRouter()
{
    RouterConfig router;
    const Database::FindResult found = RouterStore().find(&router);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the router";
        MsgBox::warning(this, tr("Failed to read the router."));
    }

    const proto::router::SessionType session_type = found == Database::FindResult::NOT_FOUND ?
        proto::router::SESSION_TYPE_OPERATOR : router.sessionType();

    ui->edit_router_address->setText(router.address());
    ui->combo_router_session_type->setCurrentIndex(ui->combo_router_session_type->findData(session_type));
    ui->edit_router_username->setText(router.username());
    ui->edit_router_password->setPassword(router.password());
    ui->checkbox_router_enabled->setChecked(router.isEnabled());
    ui->checkbox_router_sync->setChecked(router.isSyncEnabled());
    updateRouterFields();

    if (found == Database::FindResult::UNREADABLE)
        LOG(ERROR) << "Data of the router could not be read";

    ui->label_router_damaged->setVisible(found == Database::FindResult::UNREADABLE);
    router_changed_ = false;

    onRouterStatusChanged();
}

//--------------------------------------------------------------------------------------------------
bool SettingsTab::routerFromUi(std::optional<RouterConfig>* router)
{
    const QString address_text = ui->edit_router_address->text();
    const QString username = ui->edit_router_username->text();
    const SecureString password = ui->edit_router_password->password();

    if (address_text.isEmpty() && username.isEmpty() && password.isEmpty())
    {
        RouterConfig stored;
        const Database::FindResult found = RouterStore().find(&stored);
        if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::UNREADABLE)
        {
            router->reset();
            return true;
        }

        if (found == Database::FindResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the router";
            MsgBox::warning(this, tr("Failed to read the router."));
            return false;
        }
    }

    Address address = Address::fromString(address_text, kDefaultRouterClientTcpPort);
    if (!address.isValid())
    {
        LOG(ERROR) << "Invalid router address entered";
        showRouterError(ui->edit_router_address, tr("An invalid router address was entered."));
        return false;
    }

    if (!User::isValidUserName(username))
    {
        LOG(ERROR) << "Invalid user name entered";
        showRouterError(ui->edit_router_username,
                        tr("The user name can not be empty and can contain only"
                           " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        return false;
    }

    if (password.isEmpty())
    {
        LOG(ERROR) << "Invalid password entered";
        showRouterError(ui->edit_router_password, tr("Password cannot be empty."));
        return false;
    }

    RouterConfig config;
    config.setAddress(address_text);
    config.setSessionType(
        static_cast<proto::router::SessionType>(ui->combo_router_session_type->currentData().toUInt()));
    config.setUsername(username);
    config.setPassword(password);
    config.setEnabled(ui->checkbox_router_enabled->isChecked());
    config.setSyncEnabled(ui->checkbox_router_sync->isChecked());

    *router = config;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool SettingsTab::saveRouter(RouterConfig router)
{
    RouterStore store;

    RouterConfig stored;
    const Database::FindResult stored_found = store.find(&stored);
    if (stored_found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Failed to re-read the router";
        MsgBox::warning(this, tr("Failed to save the router."));
        return false;
    }

    if (stored_found == Database::FindResult::FOUND && stored.hasSameAccount(router))
    {
        if (stored.password() == router.password())
            router.setDeviceToken(stored.deviceToken());
    }

    if (stored.isSyncConfirmationNeeded(router))
    {
        if (MsgBox::question(this,
                tr("The synchronized data of this device will be sent to the storage of the account \"%1\". "
                   "Continue?").arg(router.username())) != MsgBox::Yes)
        {
            LOG(INFO) << "[ACTION] Saving the router rejected";
            return false;
        }
    }

    if (!store.save(router))
    {
        LOG(ERROR) << "Failed to write router to database";
        MsgBox::warning(this, tr("Failed to save the router."));
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::showRouterError(QLineEdit* edit, const QString& message)
{
    MsgBox::warning(this, message);
    edit->setFocus();
    edit->selectAll();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::updateRouterFields()
{
    const bool enabled = ui->checkbox_router_enabled->isChecked();

    QWidget* const widgets[] =
    {
        ui->label_router_address, ui->edit_router_address, ui->label_router_session_type,
        ui->combo_router_session_type, ui->label_router_username, ui->edit_router_username,
        ui->label_router_password, ui->edit_router_password, ui->checkbox_router_sync
    };

    for (QWidget* widget : widgets)
        widget->setEnabled(enabled);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::loadProfileList(qint64 profile_id, bool warn)
{
    QList<ProfileConfig> profiles;
    if (ProfileStore().list(&profiles) == Database::ReadResult::FAILED && warn)
        MsgBox::warning(this, tr("Failed to read the list of profiles."));

    {
        QSignalBlocker blocker(ui->combo_profile);

        const QIcon icon(":/img/dashboard-layout.svg");

        ui->combo_profile->clear();
        for (const ProfileConfig& profile : std::as_const(profiles))
        {
            ui->combo_profile->addItem(icon,
                                       profile.isDefault() ? tr("Default") : profile.name(),
                                       profile.isDefault() ? qint64(0) : profile.id());
        }

        const int index = ui->combo_profile->findData(profile_id);
        ui->combo_profile->setCurrentIndex(index >= 0 ? index : 0);
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::reloadProfiles(qint64 profile_id, bool warn)
{
    loadProfileList(profile_id, warn);
    showProfile(warn);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::showProfile(bool warn)
{
    ProfileStore store;
    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();

    proto::control::Config desktop_config;
    quint32 udp_methods = 0;

    if (profile_id <= 0)
    {
        const ProfileConfig profile = store.defaultProfile();
        desktop_config = profile.desktopConfig();
        udp_methods = profile.udpMethods();
        profile_modify_time_ = profile.modifyTime();
    }
    else
    {
        ProfileConfig profile;
        const Database::FindResult result = store.find(profile_id, &profile);
        profile_modify_time_ = profile.modifyTime();
        if (result == Database::FindResult::FOUND)
        {
            desktop_config = profile.desktopConfig();
            udp_methods = profile.udpMethods();
        }
        else
        {
            desktop_config = defaultDesktopConfig();
            udp_methods = UDP_METHOD_ALL;

            if (!warn)
                LOG(ERROR) << "Profile" << profile_id << "is not read:" << result;
            else if (result == Database::FindResult::UNREADABLE)
                MsgBox::warning(this, tr("The profile is damaged. Fix or delete it."));
            else
                MsgBox::warning(this, tr("Failed to read the profile."));
        }
    }

    ui->button_rename_profile->setEnabled(profile_id > 0);
    ui->button_delete_profile->setEnabled(profile_id > 0);

    const QList<QWidget*> controls = ui->page_desktop_content->findChildren<QWidget*>();
    std::vector<QSignalBlocker> blockers;
    blockers.reserve(static_cast<size_t>(controls.size()));
    for (QWidget* control : controls)
        blockers.emplace_back(control);

    ui->checkbox_audio->setChecked(desktop_config.audio());
    ui->checkbox_clipboard->setChecked(desktop_config.clipboard());
    ui->checkbox_cursor_shape->setChecked(desktop_config.cursor_shape());
    ui->checkbox_enable_cursor_pos->setChecked(desktop_config.cursor_position());
    ui->checkbox_desktop_effects->setChecked(!desktop_config.effects());
    ui->checkbox_desktop_wallpaper->setChecked(!desktop_config.wallpaper());
    ui->checkbox_lock_at_disconnect->setChecked(desktop_config.lock_at_disconnect());
    ui->checkbox_block_remote_input->setChecked(desktop_config.block_input());
    ui->checkbox_send_key_combinations->setChecked(!desktop_config.disable_key_combinations());

    QSize preferred(desktop_config.preferred_resolution().width(),
                    desktop_config.preferred_resolution().height());
    int resolution_index = 0;
    if (!preferred.isEmpty())
    {
        resolution_index = ui->combo_resolution->findData(preferred);
        if (resolution_index < 0)
        {
            ui->combo_resolution->addItem(
                QString("%1x%2").arg(preferred.width()).arg(preferred.height()), preferred);
            resolution_index = ui->combo_resolution->count() - 1;
        }
    }
    ui->combo_resolution->setCurrentIndex(resolution_index);

    ui->combo_quality->setCurrentIndex(ui->combo_quality->findData(desktop_config.quality()));
    ui->checkbox_preferred_size->setChecked(!desktop_config.disable_preferred_size());

    ui->checkbox_record_autostart->setChecked(desktop_config.record_sessions());
    ui->edit_record_dir->setText(recordingPath(desktop_config));

    ui->checkbox_udp_direct->setChecked(udp_methods & UDP_METHOD_DIRECT);
    ui->checkbox_udp_hole_punching->setChecked(udp_methods & UDP_METHOD_HOLE_PUNCHING);
    ui->checkbox_udp_pcp->setChecked(udp_methods & UDP_METHOD_PCP);
    ui->checkbox_udp_natpmp->setChecked(udp_methods & UDP_METHOD_NAT_PMP);
    ui->checkbox_udp_upnp->setChecked(udp_methods & UDP_METHOD_UPNP);
}

//--------------------------------------------------------------------------------------------------
ProfileConfig SettingsTab::profileFromUi() const
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
    desktop_config.set_disable_preferred_size(!ui->checkbox_preferred_size->isChecked());
    desktop_config.set_quality(ui->combo_quality->currentData().toUInt());
    desktop_config.set_disable_key_combinations(!ui->checkbox_send_key_combinations->isChecked());
    desktop_config.set_record_sessions(ui->checkbox_record_autostart->isChecked());
    desktop_config.set_recording_path(ui->edit_record_dir->text().toStdString());

    QSize preferred = ui->combo_resolution->currentData().toSize();
    if (preferred.isValid())
    {
        proto::control::Size* resolution = desktop_config.mutable_preferred_resolution();
        resolution->set_width(preferred.width());
        resolution->set_height(preferred.height());
    }

    quint32 udp_methods = 0;
    if (ui->checkbox_udp_direct->isChecked())
        udp_methods |= UDP_METHOD_DIRECT;
    if (ui->checkbox_udp_hole_punching->isChecked())
        udp_methods |= UDP_METHOD_HOLE_PUNCHING;
    if (ui->checkbox_udp_pcp->isChecked())
        udp_methods |= UDP_METHOD_PCP;
    if (ui->checkbox_udp_natpmp->isChecked())
        udp_methods |= UDP_METHOD_NAT_PMP;
    if (ui->checkbox_udp_upnp->isChecked())
        udp_methods |= UDP_METHOD_UPNP;

    ProfileConfig profile;
    profile.setDesktopConfig(desktop_config);
    profile.setUdpMethods(udp_methods);
    return profile;
}

//--------------------------------------------------------------------------------------------------
QString SettingsTab::askProfileName(const QString& title, const QString& name, qint64 own_profile_id)
{
    AutoQPointer<QInputDialog> dialog(new QInputDialog(this));
    dialog->setWindowTitle(title);
    dialog->setLabelText(tr("Name:"));
    dialog->setTextValue(name);

    for (;;)
    {
        if (dialog->exec() != QDialog::Accepted)
            return QString();

        const QString value = dialog->textValue().trimmed();
        if (value.isEmpty())
        {
            MsgBox::warning(dialog, tr("Name cannot be empty."));
            continue;
        }

        if (value.length() > ProfileConfig::kMaxNameLength)
        {
            MsgBox::warning(dialog,
                tr("Too long name. The maximum length of the name is %n characters.",
                   "", ProfileConfig::kMaxNameLength));
            continue;
        }

        bool exists = false;
        for (int i = 0; i < ui->combo_profile->count(); ++i)
        {
            if (ui->combo_profile->itemData(i).toLongLong() != own_profile_id &&
                ui->combo_profile->itemText(i).compare(value, Qt::CaseInsensitive) == 0)
            {
                exists = true;
                break;
            }
        }

        if (exists)
        {
            MsgBox::warning(dialog, tr("A profile with this name already exists."));
            continue;
        }

        return value;
    }
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::saveProfile()
{
    ProfileStore store;
    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();

    ProfileConfig profile = profileFromUi();

    if (profile_id <= 0)
    {
        ProfileConfig default_profile = store.defaultProfile();
        default_profile.setDesktopConfig(profile.desktopConfig());
        default_profile.setUdpMethods(profile.udpMethods());
        profile = default_profile;
    }
    else
    {
        profile.setId(profile_id);
        profile.setName(ui->combo_profile->currentText());
    }

    if (!store.modify(profile))
    {
        MsgBox::warning(this, tr("Unable to save profile."));
        return;
    }

    profile_modify_time_ = profile.modifyTime();

    RouterController::startSyncRecordExchange();
    emit sig_desktopConfigChanged();
}
