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

#include "client/desktop/unlock_dialog.h"

#include <QAbstractButton>
#include <QFileInfo>
#include <QIcon>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QUuid>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/application.h"
#include "client/desktop/app_lock.h"
#include "client/desktop/ui_unlock_dialog.h"
#include "client/storage/instance_settings.h"
#include "client/storage/master_password.h"
#include "client/storage/settings_store.h"
#include "client/storage/user_settings.h"
#include "common/desktop/msg_box.h"

namespace {

constexpr int kMaxNameLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
UnlockDialog::UnlockDialog(Mode mode, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UnlockDialog>()),
      mode_(mode)
{
    LOG(TRACE) << "Ctor";
    ui->setupUi(this);

#if !defined(Q_OS_MACOS)
    const QSize icon_size(24, 24);
    ui->button_create->setIconSize(icon_size);
    ui->button_rename->setIconSize(icon_size);
    ui->button_delete->setIconSize(icon_size);
#endif // !defined(Q_OS_MACOS)

    if (mode_ == Mode::STARTUP)
        setWindowTitle(tr("Open Database"));

    ui->label_icon->setPixmap(GuiApplication::svgPixmap(":/img/lock.svg", QSize(48, 48)));
    ui->edit_name->setMaxLength(kMaxNameLength);
    ui->edit_password->setShowPasswordButtonVisible(true);
    ui->edit_confirm->setShowPasswordButtonVisible(true);
    ui->combo_database->setEnabled(mode_ == Mode::STARTUP);

    connect(ui->combo_database, &QComboBox::currentIndexChanged, ui->edit_password, &QLineEdit::clear);
    connect(ui->combo_database, &QComboBox::currentIndexChanged, this, &UnlockDialog::onDatabaseChanged);
    connect(ui->button_create, &QToolButton::clicked, this, &UnlockDialog::onCreate);
    connect(ui->button_rename, &QToolButton::clicked, this, &UnlockDialog::onRename);
    connect(ui->button_delete, &QToolButton::clicked, this, &UnlockDialog::onDelete);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button)
    {
        if (ui->button_box->standardButton(button) == QDialogButtonBox::Ok)
        {
            if (isCreating())
                create();
            else if (isRenaming())
                rename();
            else
                unlock();
        }
        else
        {
            reject();
        }
    });

    reload();
}

//--------------------------------------------------------------------------------------------------
UnlockDialog::~UnlockDialog()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::reject()
{
    const bool has_list = databases_.size() > 1 || (!databases_.isEmpty() && databases_.first().has_password);

    if (isCreating() && has_list)
    {
        ui->edit_password->clear();
        ui->edit_confirm->clear();
        setCreating(false);
    }
    else if (isRenaming())
    {
        setCreating(false);
    }
    else
    {
        QDialog::reject();
    }
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::closeEvent(QCloseEvent* event)
{
    if (event->spontaneous())
        QDialog::closeEvent(event);
    else
        QDialog::reject();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::showEvent(QShowEvent* event)
{
    if (isCreating() && ui->edit_name->text().isEmpty())
        ui->edit_name->setFocus();
    else if (!ui->edit_password->isHidden())
        ui->edit_password->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onDatabaseChanged()
{
    const int index = ui->combo_database->currentIndex();
    if (index < 0 || index >= databases_.size())
        return;

    const Database::Info& info = databases_.at(index);
    const bool is_open = open_ids_.contains(info.id);

    ui->button_rename->setEnabled(!is_open);
    ui->button_delete->setEnabled(!is_open);
    setPasswordVisible(!is_open && info.has_password && (mode_ == Mode::LOCK || !info.auto_unlock));

    if (is_open)
        ui->label_description->setText(tr("The database is already open."));
    else if (!info.has_password)
        ui->label_description->setText(tr("The master password of this database is not set."));
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onCreate()
{
    ui->edit_name->clear();
    ui->edit_password->clear();
    ui->edit_confirm->clear();

    setCreating(true);
    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onRename()
{
    const int index = ui->combo_database->currentIndex();
    if (index < 0 || index >= databases_.size())
        return;

    const Database::Info& info = databases_.at(index);

    if (Application::instance()->hasInstance(info.id))
    {
        MsgBox::warning(this, tr("The database is open in another window. Close it to rename the database."));
        return;
    }

    ui->formLayout->setRowVisible(ui->layout_database, false);
    ui->formLayout->setRowVisible(ui->edit_name, true);

    setPasswordVisible(false);
    ui->label_description->setText(tr("Enter the new name of the database."));

    ui->edit_name->setText(info.name);
    ui->edit_name->selectAll();
    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::onDelete()
{
    const int index = ui->combo_database->currentIndex();
    if (index < 0 || index >= databases_.size())
        return;

    const Database::Info info = databases_.at(index);

    if (Application::instance()->hasInstance(info.id))
    {
        MsgBox::warning(this, tr("The database is open in another window. Close it to delete the database."));
        return;
    }

    const QString question =
        tr("The database \"%1\" with all its records and settings will be deleted.\nDelete the database?")
            .arg(info.name.isEmpty() ? tr("Unnamed database") : info.name);

    if (MsgBox::importantQuestion(this, question, Seconds(10)) != MsgBox::Yes)
        return;

    if (!Database::moveToTrash(info.id))
    {
        if (MsgBox::question(this, tr("Unable to move the database to the trash. Delete it permanently?")) !=
            MsgBox::Yes)
        {
            return;
        }

        if (!Database::remove(info.id))
        {
            LOG(ERROR) << "Unable to delete the database";
            MsgBox::warning(this, tr("Unable to delete the database."));
            return;
        }
    }

    InstanceSettings::remove(info.id);

    LOG(INFO) << "Database deleted by user";
    ui->edit_password->clear();
    reload();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::reload()
{
    databases_ = Database::list();
    open_ids_.clear();

    QString selected_id = QFileInfo(Database::filePath()).completeBaseName();
    if (selected_id.isEmpty())
        selected_id = UserSettings().lastDatabase();

    int selected_index = 0;

    {
        const QSignalBlocker blocker(ui->combo_database);
        const QIcon icon(":/img/database.svg");

        ui->combo_database->clear();

        for (int i = 0; i < databases_.size(); ++i)
        {
            const Database::Info& info = databases_.at(i);
            const QString name = info.name.isEmpty() ? tr("Unnamed database") : info.name;

            if (mode_ == Mode::STARTUP && Application::instance()->hasInstance(info.id))
            {
                open_ids_.insert(info.id);
                ui->combo_database->addItem(icon, tr("%1 (open)").arg(name));
            }
            else
            {
                ui->combo_database->addItem(icon, name);
            }

            if (info.id == selected_id)
                selected_index = i;
        }

        ui->combo_database->setCurrentIndex(selected_index);
    }

    if (databases_.size() == 1 && !databases_.first().has_password)
    {
        requestNewPassword(databases_.first());
        return;
    }

    if (databases_.isEmpty())
        ui->edit_name->setText(tr("Database #1"));

    setCreating(databases_.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::unlock()
{
    const int index = ui->combo_database->currentIndex();
    if (index < 0 || index >= databases_.size())
        return;

    const QString id = databases_.at(index).id;

    if (open_ids_.contains(id))
    {
        Application* application = Application::instance();

        if (!application->hasInstance(id))
        {
            LOG(INFO) << "The database is no longer open";

            Database::select(id);
            reload();
            return;
        }

        LOG(INFO) << "The database is already open, activating its window";
        application->setInstanceKey(id);
        application->activateWindow();

        reject();
        return;
    }

    if (mode_ == Mode::STARTUP)
        Database::select(id);

    if (!Database::instance().isValid())
    {
        LOG(ERROR) << "Unable to open the database";
        MsgBox::warning(this, tr("Unable to open the database."));
        return;
    }

    if (!MasterPassword::isSet())
    {
        requestNewPassword(databases_.at(index));
        return;
    }

    if (ui->edit_password->isHidden())
    {
        if (MasterPassword::autoUnlock())
        {
            accept();
            return;
        }

        setPasswordVisible(true);
        ui->edit_password->setFocus();
        return;
    }

    const MasterPassword::Result result = MasterPassword::unlock(ui->edit_password->password());
    if (result == MasterPassword::Result::SUCCESS)
    {
        accept();
    }
    else if (result == MasterPassword::Result::INVALID_PASSWORD)
    {
        MsgBox::warning(this, tr("Invalid master password."));
        ui->edit_password->selectAll();
        ui->edit_password->setFocus();
    }
    else
    {
        LOG(ERROR) << "Unable to unlock the database";
        MsgBox::warning(this, tr("Unable to unlock the database."));
    }
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::create()
{
    const QString name = ui->edit_name->text().trimmed();
    if (!isNameAccepted(name, no_password_id_))
    {
        ui->edit_name->setFocus();
        return;
    }

    const SecureString password = ui->edit_password->password();
    if (password != ui->edit_confirm->password())
    {
        MsgBox::warning(this, tr("Passwords do not match."));
        ui->edit_confirm->setFocus();
        return;
    }

    if (!AppLock::checkNewPassword(this, password))
    {
        ui->edit_password->setFocus();
        return;
    }

    QString id = no_password_id_;
    if (id.isEmpty())
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    Database::select(id);

    if (MasterPassword::setNew(password) != MasterPassword::Result::SUCCESS ||
        !SettingsStore().setName(name))
    {
        LOG(ERROR) << "Unable to create the database";

        if (no_password_id_.isEmpty())
            Database::remove(id);

        MsgBox::warning(this, tr("Unable to create the database."));
        return;
    }

    LOG(INFO) << "Database created";
    accept();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::rename()
{
    const int index = ui->combo_database->currentIndex();
    if (index < 0 || index >= databases_.size())
        return;

    const QString id = databases_.at(index).id;

    const QString name = ui->edit_name->text().trimmed();
    if (!isNameAccepted(name, id))
    {
        ui->edit_name->setFocus();
        return;
    }

    Database::select(id);

    if (!SettingsStore().setName(name))
    {
        LOG(ERROR) << "Unable to rename the database";
        MsgBox::warning(this, tr("Unable to rename the database."));
        return;
    }

    LOG(INFO) << "Database renamed by user";
    reload();
}

//--------------------------------------------------------------------------------------------------
bool UnlockDialog::isNameAccepted(const QString& name, const QString& own_id)
{
    if (name.isEmpty())
    {
        MsgBox::warning(this, tr("Name cannot be empty."));
        return false;
    }

    if (name.length() > kMaxNameLength)
    {
        MsgBox::warning(this,
            tr("Too long name. The maximum length of the name is %n characters.", "", kMaxNameLength));
        return false;
    }

    for (const Database::Info& info : std::as_const(databases_))
    {
        if (info.id != own_id && info.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            MsgBox::warning(this, tr("A database with this name already exists."));
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool UnlockDialog::isCreating() const
{
    return !ui->edit_confirm->isHidden();
}

//--------------------------------------------------------------------------------------------------
bool UnlockDialog::isRenaming() const
{
    return !ui->edit_name->isHidden() && ui->edit_confirm->isHidden();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::setCreating(bool creating)
{
    ui->formLayout->setRowVisible(ui->layout_database, !creating);
    ui->formLayout->setRowVisible(ui->edit_name, creating);
    ui->formLayout->setRowVisible(ui->edit_confirm, creating);
    ui->button_create->setVisible(!creating && mode_ == Mode::STARTUP);
    ui->button_rename->setVisible(!creating && mode_ == Mode::STARTUP);
    ui->button_delete->setVisible(!creating && mode_ == Mode::STARTUP);

    if (!creating)
    {
        no_password_id_.clear();
        onDatabaseChanged();
        return;
    }

    setPasswordVisible(true);
    ui->label_description->setText(tr("Enter the name and the master password of the new database."));
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::requestNewPassword(const Database::Info& info)
{
    no_password_id_ = info.id;

    ui->edit_name->setText(info.name);
    ui->edit_password->clear();
    ui->edit_confirm->clear();

    setCreating(true);

    if (info.name.isEmpty())
        ui->edit_name->setFocus();
    else
        ui->edit_password->setFocus();
}

//--------------------------------------------------------------------------------------------------
void UnlockDialog::setPasswordVisible(bool visible)
{
    ui->formLayout->setRowVisible(ui->edit_password, visible);

    if (visible)
        ui->label_description->setText(tr("Enter the master password to unlock the database."));
    else
        ui->label_description->setText(tr("The master password of this database is remembered on this computer."));

    QTimer::singleShot(MilliSeconds(0), this, [this]() { setFixedHeight(sizeHint().height()); });
}
