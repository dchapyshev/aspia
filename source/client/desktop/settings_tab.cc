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
#include <QHeaderView>
#include <QInputDialog>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/auto_qpointer.h"
#include "base/build_config.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/crypto/secure_string.h"
#include "base/net/udp_channel.h"
#include "client/application.h"
#include "client/mcp/mcp_worker.h"
#include "client/desktop/router_dialog.h"
#include "client/desktop/ui_settings_tab.h"
#include "client/desktop/update_server_dialog.h"
#include "client/storage/database.h"
#include "client/storage/master_password.h"
#include "client/storage/settings.h"
#include "client/storage/system_settings.h"
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
        button->setFixedSize(120, 64);
        category_group_->addButton(button, id);
        // Insert before the trailing spacer (last item in layout).
        layout->insertWidget(layout->count() - 1, button, 0, Qt::AlignHCenter);
    };

    int button_id = 0;
    add_button(button_id++, ":/img/gear.svg", tr("General"));
    add_button(button_id++, ":/img/stack.svg", tr("Routers"));
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
    center(ui->layout_routers_outer, ui->page_routers_content, kMaxContentWidth);
    center(ui->layout_desktop_outer, ui->page_desktop_content, kMaxContentWidth);
    center(ui->layout_mcp_outer, ui->page_mcp_content, kMaxContentWidth);
    center(ui->layout_update_outer, ui->page_update_content, kMaxContentWidth);

    // A scroll area fills the page with the window color, the pane of the tab has the base one.
    for (QScrollArea* page : { ui->page_general, ui->page_routers, ui->page_desktop, ui->page_mcp, ui->page_update })
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

    ui->edit_display_name->setText(db.displayName());
    ui->checkbox_confirm_close->setChecked(db.isConfirmCloseEnabled());

    ui->checkbox_backup_on_startup->setChecked(db.isBackupOnStartupEnabled());
    ui->edit_backup_dir->setText(db.backupPath());

    ui->combo_backup_retention->addItem(tr("1 week"), static_cast<int>(AutoBackup::Retention::ONE_WEEK));
    ui->combo_backup_retention->addItem(tr("2 weeks"), static_cast<int>(AutoBackup::Retention::TWO_WEEKS));
    ui->combo_backup_retention->addItem(tr("1 month"), static_cast<int>(AutoBackup::Retention::ONE_MONTH));
    ui->combo_backup_retention->addItem(tr("6 months"), static_cast<int>(AutoBackup::Retention::SIX_MONTHS));
    ui->combo_backup_retention->addItem(tr("1 year"), static_cast<int>(AutoBackup::Retention::ONE_YEAR));
    ui->combo_backup_retention->setCurrentIndex(
        ui->combo_backup_retention->findData(static_cast<int>(db.backupRetention())));

    ui->combo_lock_timeout->addItem(tr("Do not lock"), 0);
    ui->combo_lock_timeout->addItem(tr("1 minute"), 1);
    ui->combo_lock_timeout->addItem(tr("5 minutes"), 5);
    ui->combo_lock_timeout->addItem(tr("10 minutes"), 10);
    ui->combo_lock_timeout->addItem(tr("30 minutes"), 30);
    ui->combo_lock_timeout->addItem(tr("1 hour"), 60);

    const int lock_index = ui->combo_lock_timeout->findData(static_cast<int>(db.lockTimeout().count()));
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
    db.proxyConfig(&proxy);
    const ProxySettings& proxy_settings = proxy.settings();
    ui->combo_proxy_type->setCurrentIndex(
        ui->combo_proxy_type->findData(proxy_settings.enabled ? static_cast<int>(proxy_settings.type) : kNoProxy));
    ui->edit_proxy_address->setText(proxy_settings.address);
    if (proxy_settings.port)
        ui->spinbox_proxy_port->setValue(proxy_settings.port);
    ui->edit_proxy_username->setText(proxy_settings.user);
    ui->edit_proxy_password->setPassword(proxy_settings.password);
    updateProxyFields();

    // Routers page.
    QHeaderView* routers_header = ui->tree_routers->header();
    routers_header->resizeSection(0, 200);
    routers_header->resizeSection(1, 200);
    routers_header->resizeSection(2, 150);
    routers_header->resizeSection(3, 150);

    reloadRouters(-1);

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
    ui->checkbox_mcp_enabled->setChecked(db.isMcpEnabled());
    ui->checkbox_mcp_auto_confirm->setChecked(db.isMcpAutoConfirmEnabled());
    ui->checkbox_mcp_sign_in->setChecked(db.isMcpSignInEnabled());

    mcp_session_types_ = new QButtonGroup(this);
    mcp_session_types_->setExclusive(false);

    const quint32 mcp_session_types = db.mcpSessionTypes();
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
    ui->checkbox_check_updates->setChecked(db.isCheckUpdatesEnabled());

    ui->combobox_update_channel->addItem(tr("Stable"), kStableUpdateChannel);
    ui->combobox_update_channel->addItem(tr("Beta"), kBetaUpdateChannel);
    ui->combobox_update_channel->addItem(tr("Alpha"), kAlphaUpdateChannel);

    int channel_index = ui->combobox_update_channel->findData(db.updateChannel());
    ui->combobox_update_channel->setCurrentIndex(channel_index >= 0 ? channel_index : 0);

    loadUpdateServer();

    // Wire signals after initial values are loaded to avoid spurious saves.
    connect(category_group_, &QButtonGroup::idClicked, this, &SettingsTab::onCategoryChanged);

    connect(ui->combo_language, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onLanguageChanged);
    connect(ui->combo_theme, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onThemeChanged);
    connect(ui->edit_display_name, &QLineEdit::editingFinished,
            this, &SettingsTab::onDisplayNameChanged);
    connect(ui->checkbox_confirm_close, &QCheckBox::toggled, this, &SettingsTab::onConfirmCloseChanged);
    connect(ui->checkbox_backup_on_startup, &QCheckBox::toggled, this, &SettingsTab::onBackupOnStartupChanged);
    connect(ui->edit_backup_dir, &QLineEdit::editingFinished, this, &SettingsTab::onBackupPathChanged);
    connect(ui->button_select_backup_dir, &QPushButton::clicked, this, &SettingsTab::onSelectBackupPath);
    connect(ui->combo_backup_retention, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onBackupRetentionChanged);
    connect(ui->combo_proxy_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onProxyChanged);
    connect(ui->edit_proxy_address, &QLineEdit::editingFinished, this, &SettingsTab::onProxyChanged);
    connect(ui->spinbox_proxy_port, &QSpinBox::editingFinished, this, &SettingsTab::onProxyChanged);
    connect(ui->edit_proxy_username, &QLineEdit::editingFinished, this, &SettingsTab::onProxyChanged);
    connect(ui->edit_proxy_password, &QLineEdit::editingFinished, this, &SettingsTab::onProxyChanged);
    connect(ui->combo_lock_timeout, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onLockTimeoutChanged);
    connect(ui->checkbox_auto_unlock, &QCheckBox::toggled, this, &SettingsTab::onAutoUnlockChanged);
    connect(ui->button_change_master_password, &QPushButton::clicked,
            this, &SettingsTab::onChangeMasterPassword);

    connect(ui->tree_routers, &QTreeWidget::currentItemChanged, this, &SettingsTab::onCurrentRouterChanged);
    connect(ui->tree_routers, &QTreeWidget::itemDoubleClicked, this, &SettingsTab::onEditRouter);
    connect(ui->button_add_router, &QPushButton::clicked, this, &SettingsTab::onAddRouter);
    connect(ui->button_edit_router, &QPushButton::clicked, this, &SettingsTab::onEditRouter);
    connect(ui->button_delete_router, &QPushButton::clicked, this, &SettingsTab::onDeleteRouter);

    connect(ui->combo_profile, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsTab::onProfileChanged);
    connect(ui->button_add_profile, &QPushButton::clicked, this, &SettingsTab::onAddProfile);
    connect(ui->button_rename_profile, &QPushButton::clicked, this, &SettingsTab::onRenameProfile);
    connect(ui->button_delete_profile, &QPushButton::clicked, this, &SettingsTab::onDeleteProfile);

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
    connect(ui->button_select_record_dir, &QPushButton::clicked, this, &SettingsTab::onSelectRecordingPath);

    connect(ui->checkbox_udp_direct, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_hole_punching, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_pcp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_natpmp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);
    connect(ui->checkbox_udp_upnp, &QCheckBox::toggled, this, &SettingsTab::onDesktopFeatureChanged);

    connect(ui->checkbox_mcp_enabled, &QCheckBox::toggled, this, &SettingsTab::onMcpEnabledChanged);
    connect(ui->checkbox_mcp_auto_confirm, &QCheckBox::toggled, this, &SettingsTab::onMcpAutoConfirmChanged);
    connect(ui->checkbox_mcp_sign_in, &QCheckBox::toggled, this, &SettingsTab::onMcpSignInChanged);
    connect(mcp_session_types_, &QButtonGroup::idToggled, this, &SettingsTab::onMcpSessionTypesChanged);
    connect(ui->button_copy_mcp_command, &QPushButton::clicked, this, &SettingsTab::onCopyMcpCommand);

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
void SettingsTab::activate()
{
    QTreeWidgetItem* item = ui->tree_routers->currentItem();
    reloadRouters(item ? item->data(0, Qt::UserRole).toLongLong() : -1);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::deactivate()
{
    // Nothing
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
void SettingsTab::onConfirmCloseChanged()
{
    LOG(INFO) << "[ACTION] Confirm close changed";
    Database::instance().setConfirmCloseEnabled(ui->checkbox_confirm_close->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onBackupOnStartupChanged()
{
    LOG(INFO) << "[ACTION] Backup on startup changed";
    Database::instance().setBackupOnStartupEnabled(ui->checkbox_backup_on_startup->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onBackupPathChanged()
{
    LOG(INFO) << "[ACTION] Backup path changed";

    Database& db = Database::instance();
    if (ui->edit_backup_dir->text().isEmpty())
    {
        ui->edit_backup_dir->setText(db.backupPath());
        return;
    }

    db.setBackupPath(ui->edit_backup_dir->text());
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
    Database::instance().setBackupPath(path);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onBackupRetentionChanged()
{
    LOG(INFO) << "[ACTION] Backup retention changed";
    Database::instance().setBackupRetention(
        static_cast<AutoBackup::Retention>(ui->combo_backup_retention->currentData().toInt()));
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onProxyChanged()
{
    const int type = ui->combo_proxy_type->currentData().toInt();

    ProxySettings settings;
    settings.enabled = type != kNoProxy;
    if (settings.enabled)
        settings.type = static_cast<ProxySettings::Type>(type);
    settings.address = ui->edit_proxy_address->text().trimmed();
    settings.port = static_cast<quint16>(ui->spinbox_proxy_port->value());
    settings.user = ui->edit_proxy_username->text();
    settings.password = ui->edit_proxy_password->password();

    ProxyConfig proxy;
    proxy.setSettings(settings);

    if (settings.enabled)
        LOG(INFO) << "[ACTION] Proxy changed:" << settings.type;
    else
        LOG(INFO) << "[ACTION] Proxy turned off";

    updateProxyFields();
    Database::instance().setProxyConfig(proxy);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onLockTimeoutChanged()
{
    LOG(INFO) << "[ACTION] Lock timeout changed";
    Database::instance().setLockTimeout(Minutes(ui->combo_lock_timeout->currentData().toInt()));
    emit sig_lockTimeoutChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onAutoUnlockChanged()
{
    const bool enable = ui->checkbox_auto_unlock->isChecked();
    LOG(INFO) << "[ACTION] Auto unlock changed:" << enable;

    const bool done = enable ? enableAutoUnlock() : MasterPassword::setAutoUnlockEnabled(false);
    const bool auto_unlock = MasterPassword::isAutoUnlockEnabled();

    if (!done)
    {
        QSignalBlocker blocker(ui->checkbox_auto_unlock);
        ui->checkbox_auto_unlock->setChecked(auto_unlock);
    }

    // A locked application would be unlocked by starting it again.
    if (auto_unlock)
        ui->combo_lock_timeout->setCurrentIndex(ui->combo_lock_timeout->findData(0));
    ui->combo_lock_timeout->setEnabled(!auto_unlock);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCurrentRouterChanged()
{
    const bool selected = ui->tree_routers->currentItem() != nullptr;
    ui->button_edit_router->setEnabled(selected);
    ui->button_delete_router->setEnabled(selected);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onAddRouter()
{
    LOG(INFO) << "[ACTION] Add router";

    AutoQPointer<RouterDialog> dialog(new RouterDialog(-1, this));
    if (dialog->exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadRouters(-1);
    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onEditRouter()
{
    QTreeWidgetItem* item = ui->tree_routers->currentItem();
    if (!item)
        return;

    const qint64 router_id = item->data(0, Qt::UserRole).toLongLong();
    LOG(INFO) << "[ACTION] Edit router" << router_id;

    AutoQPointer<RouterDialog> dialog(new RouterDialog(router_id, this));
    if (dialog->exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadRouters(router_id);
    emit sig_routersChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onDeleteRouter()
{
    QTreeWidgetItem* item = ui->tree_routers->currentItem();
    if (!item)
        return;

    const qint64 router_id = item->data(0, Qt::UserRole).toLongLong();
    LOG(INFO) << "[ACTION] Delete router" << router_id;

    QString message = tr("Are you sure you want to delete router \"%1\"?").arg(item->text(0));
    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "Action is rejected by user";
        return;
    }

    if (!Database::instance().removeRouter(router_id))
    {
        MsgBox::warning(this, tr("Unable to delete router."));
        return;
    }

    Settings().removeRouterExpanded(router_id);
    reloadRouters(-1);
    emit sig_routersChanged();
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

    if (!Database::instance().addProfile(profile))
    {
        MsgBox::warning(this, tr("Unable to add profile."));
        return;
    }

    reloadProfiles(profile.id());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onRenameProfile()
{
    LOG(INFO) << "[ACTION] Rename profile";

    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();
    if (profile_id <= 0)
        return;

    Database& db = Database::instance();

    ProfileConfig profile;
    if (db.findProfile(profile_id, &profile) != Database::FindResult::FOUND)
    {
        MsgBox::warning(this, tr("Failed to read the profile."));
        return;
    }

    const QString name = askProfileName(tr("Rename Profile"), profile.name(), ui->combo_profile->currentIndex());
    if (name.isEmpty())
        return;

    profile.setName(name);

    if (!db.modifyProfile(profile))
    {
        MsgBox::warning(this, tr("Unable to rename profile."));
        return;
    }

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

    if (!Database::instance().removeProfile(profile_id))
    {
        MsgBox::warning(this, tr("Unable to delete profile."));
        return;
    }

    reloadProfiles(0);
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

    Database::instance().setMcpEnabled(enable);
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
    Database::instance().setMcpAutoConfirmEnabled(ui->checkbox_mcp_auto_confirm->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onMcpSignInChanged()
{
    LOG(INFO) << "[ACTION] MCP sign in changed";
    Database::instance().setMcpSignInEnabled(ui->checkbox_mcp_sign_in->isChecked());
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

    Database::instance().setMcpSessionTypes(session_types);
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
    Database::instance().setCheckUpdatesEnabled(ui->checkbox_check_updates->isChecked());
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onUpdateChannelChanged()
{
    QString channel = ui->combobox_update_channel->currentData().toString();
    LOG(INFO) << "[ACTION] Update channel changed:" << channel;
    Database::instance().setUpdateChannel(channel);
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::onCheckForUpdatesClicked()
{
    LOG(INFO) << "[ACTION] Check for updates";

    ProxyConfig proxy;
    if (Database::instance().proxyConfig(&proxy) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "No readable proxy config";
        MsgBox::warning(this, tr("Unable to read the proxy settings."));
        return;
    }

    SystemSettings settings;
    AutoQPointer<UpdateDialog> dialog(new UpdateDialog(
        settings.updateServer(), settings.updatePublicKey(), ui->combobox_update_channel->currentData().toString(),
        "client", proxy.settings(), UpdateDialog::Action::ASK, this));
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
void SettingsTab::reloadRouters(qint64 router_id)
{
    QList<RouterConfig> routers;
    if (Database::instance().routerList(&routers) == Database::ReadResult::FAILED)
        MsgBox::warning(this, tr("Failed to read the list of routers."));

    QTreeWidget* tree = ui->tree_routers;
    tree->clear();

    const QIcon icon(":/img/stack.svg");

    for (const RouterConfig& router : std::as_const(routers))
    {
        QString access_level;

        if (router.sessionType() == proto::router::SESSION_TYPE_ADMIN)
            access_level = tr("Administrator");
        else if (router.sessionType() == proto::router::SESSION_TYPE_MANAGER)
            access_level = tr("Manager");
        else if (router.sessionType() == proto::router::SESSION_TYPE_OPERATOR)
            access_level = tr("Operator");

        QTreeWidgetItem* item = new QTreeWidgetItem(tree);
        item->setData(0, Qt::UserRole, router.routerId());
        item->setIcon(0, icon);
        item->setText(0, router.displayLabel());
        item->setText(1, router.address());
        item->setText(2, router.username());
        item->setText(3, access_level);

        if (router.routerId() == router_id)
            tree->setCurrentItem(item);
    }

    onCurrentRouterChanged();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::reloadProfiles(qint64 profile_id)
{
    QList<ProfileConfig> profiles;
    if (Database::instance().profileList(&profiles) == Database::ReadResult::FAILED)
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

    showProfile();
}

//--------------------------------------------------------------------------------------------------
void SettingsTab::showProfile()
{
    Database& db = Database::instance();
    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();

    proto::control::Config desktop_config;
    quint32 udp_methods = 0;

    if (profile_id <= 0)
    {
        const ProfileConfig profile = db.defaultProfile();
        desktop_config = profile.desktopConfig();
        udp_methods = profile.udpMethods();
    }
    else
    {
        ProfileConfig profile;
        const Database::FindResult result = db.findProfile(profile_id, &profile);
        if (result == Database::FindResult::FOUND)
        {
            desktop_config = profile.desktopConfig();
            udp_methods = profile.udpMethods();
        }
        else
        {
            desktop_config = defaultDesktopConfig();
            udp_methods = UDP_METHOD_ALL;

            if (result == Database::FindResult::UNREADABLE)
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
QString SettingsTab::askProfileName(const QString& title, const QString& name, int own_index)
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
            if (i != own_index && ui->combo_profile->itemText(i).compare(value, Qt::CaseInsensitive) == 0)
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
    Database& db = Database::instance();
    const qint64 profile_id = ui->combo_profile->currentData().toLongLong();

    ProfileConfig profile = profileFromUi();

    if (profile_id <= 0)
    {
        ProfileConfig default_profile = db.defaultProfile();
        default_profile.setDesktopConfig(profile.desktopConfig());
        default_profile.setUdpMethods(profile.udpMethods());
        profile = default_profile;
    }
    else
    {
        profile.setId(profile_id);
        profile.setName(ui->combo_profile->currentText());
    }

    if (!db.modifyProfile(profile))
    {
        MsgBox::warning(this, tr("Unable to save profile."));
        return;
    }

    emit sig_desktopConfigChanged();
}
