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

#include "common/desktop/update_dialog.h"

#include <QCloseEvent>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "common/update_checker.h"
#include "common/update_installer.h"
#include "common/desktop/download_dialog.h"
#include "common/desktop/elevate_util.h"
#include "common/desktop/msg_box.h"
#include "ui_update_dialog.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString makeUrl(const QUrl& url)
{
    return QString("<a href='%1'>%1</a>").arg(url.toString());
}

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateDialog::UpdateDialog(const QString& server, const QString& package, Action action,
                           QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>()),
      server_(server),
      action_(action)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->label_icon->setFixedSize(QSize(32, 32));
    ui->label_icon->setPixmap(GuiApplication::svgPixmap(":/img/restart.svg", QSize(32, 32)));

    connect(ui->button_update, &QPushButton::clicked, this, &UpdateDialog::onUpdateNow);
    connect(ui->button_close, &QPushButton::clicked, this, &UpdateDialog::close);

    ui->label_current->setText(kCurrentVersion.toString());
    ui->label_available->setText(tr("Receiving information..."));

    checker_ = std::make_unique<UpdateChecker>(server, package);

    connect(checker_.get(), &UpdateChecker::sig_checkFinished,
            this, &UpdateDialog::onUpdateCheckFinished);
    connect(checker_.get(), &UpdateChecker::sig_checkFailed,
            this, &UpdateDialog::onUpdateCheckFailed);

    checker_->start();
}

//--------------------------------------------------------------------------------------------------
UpdateDialog::~UpdateDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        LOG(INFO) << "[ACTION] Escape key pressed";
        close();
        return;
    }

    QDialog::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::closeEvent(QCloseEvent* event)
{
    LOG(INFO) << "Close event";

    if (installer_ || elevate_util_)
    {
        event->ignore();
        return;
    }

    if (checker_)
    {
        LOG(INFO) << "Distroy checker...";
        checker_.reset();
    }

    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateNow()
{
    LOG(INFO) << "[ACTION] Update now";

    if (!UpdateInstaller::isSupported(update_info_.format()))
    {
        MsgBox::information(this, tr("Install the update yourself: %1").arg(update_info_.url()));
        return;
    }

    if (action_ == Action::ASK)
    {
        QString message1 = tr("An update will be downloaded. After the download is complete, the "
                              "application will automatically close.");
        QString message2 = tr("All connected sessions will be terminated. You cannot establish a "
                              "connection until the update is complete.");
        QString message3 = tr("All unsaved data will be lost.");
        QString question = tr("Continue?");

        if (MsgBox::question(this, QString("%1\n%2\n%3\n%4")
            .arg(message1, message2, message3, question)) != MsgBox::Yes)
        {
            return;
        }

        LOG(INFO) << "[ACTION] Update confirmed by user";
    }

    // Downloading the package and checking it against the manifest is what the privileges are for,
    // so an unprivileged process hands over the whole thing instead of doing part of it.
    if (ElevateUtil::isPrivileged())
        startInstall();
    else
        startPrivilegedInstance();
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::startInstall()
{
    installer_.reset(new UpdateInstaller(UpdateInstaller::Mode::USER, this));

    connect(installer_, &UpdateInstaller::sig_finished, this,
            [this](bool success, const QString& error)
    {
        if (success)
        {
            accept();
            return;
        }

        if (!error.isEmpty())
            MsgBox::warning(this, tr("An error occurred while installing the update: %1").arg(error));

        setInstalling(false);
    });

    QString file_path = installer_->createPackageFile(update_info_);
    if (file_path.isEmpty())
    {
        setInstalling(false);
        MsgBox::warning(this, tr("An error occurred while installing the update."));
        return;
    }

    setInstalling(true);

    // The download reports its own errors and its own cancellation.
    if (DownloadDialog(update_info_.url(), file_path, this).exec() != DownloadDialog::Accepted)
    {
        setInstalling(false);
        return;
    }

    UpdateInstaller::Result result = installer_->install();
    if (result != UpdateInstaller::Result::STARTED)
    {
        setInstalling(false);

        if (result == UpdateInstaller::Result::DAMAGED)
            MsgBox::warning(this, tr("The downloaded file is damaged."));
        else
            MsgBox::warning(this, tr("An error occurred while installing the update."));
    }
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::startPrivilegedInstance()
{
    elevate_util_ = ElevateUtil::create(this);
    if (!elevate_util_)
    {
        MsgBox::warning(this, tr("An error occurred while installing the update."));
        return;
    }

    QStringList arguments;
    arguments << "--update=" + server_;
    arguments << "--locale=" + GuiApplication::instance()->locale();

    // The started instance has its own windows and reports what went wrong itself.
    bool started = elevate_util_->runElevated(arguments, winId(), [this](int exit_code)
    {
        LOG(INFO) << "Privileged instance finished with code:" << exit_code;

        if (exit_code == kInstalledExitCode)
        {
            accept();
            return;
        }

        setInstalling(false);

        if (exit_code != kClosedExitCode && exit_code != ElevateUtil::kDeclinedExitCode)
            MsgBox::warning(this, tr("An error occurred while installing the update."));
    });

    if (!started)
    {
        elevate_util_.reset();
        return;
    }

    setInstalling(true);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateCheckFinished(const UpdateInfo& update_info)
{
    update_info_ = update_info;

    if (!update_info_.isValid())
    {
        LOG(INFO) << "No updates available";

        ui->label_available->setText(kCurrentVersion.toString());
        ui->edit_description->setText(tr("No updates available."));
    }
    else
    {
        LOG(INFO) << "New version available:" << update_info_.version().toString();

        ui->label_available->setText(update_info_.version().toString());
        ui->edit_description->setText(update_info_.description());
        ui->label_url->setText(makeUrl(update_info_.url()));

        ui->button_update->setEnabled(true);

        if (action_ == Action::INSTALL)
            onUpdateNow();
    }

    destroyChecker();
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateCheckFailed()
{
    LOG(ERROR) << "Error while retrieving update information";

    ui->label_available->setText(tr("Unknown"));
    ui->edit_description->setText(tr("Error retrieving update information."));

    destroyChecker();
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::setInstalling(bool installing)
{
    if (installing)
    {
        ui->edit_description->setText(tr("Installing the update. Please wait."));
    }
    else
    {
        ui->edit_description->setText(update_info_.description());
        installer_.reset();
        elevate_util_.reset();
    }

    ui->button_update->setEnabled(!installing);
    ui->button_close->setEnabled(!installing);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::destroyChecker()
{
    QTimer::singleShot(MilliSeconds(0), this, [this]()
    {
        LOG(INFO) << "Destroy update checker";
        checker_.reset();
    });
}
