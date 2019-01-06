//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "updater/update_dialog.h"

#include <QDir>
#include <QMessageBox>
#include <QTemporaryFile>

#include "base/win/process_util.h"
#include "build/version.h"
#include "updater/download_dialog.h"
#include "updater/update_checker.h"
#include "ui_update_dialog.h"

namespace aspia {

namespace {

QString makeUrl(const QUrl& url)
{
    return QString("<a href='%1'>%1</a>").arg(url.toString());
}

} // namespace

UpdateDialog::UpdateDialog(const QString& update_server,
                           const QString& package_name,
                           QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>())
{
    initialize();

    UpdateChecker* update_checher = new UpdateChecker(this);

    connect(update_checher, &UpdateChecker::finished,
            [this](const UpdateInfo& update_info)
    {
        QVersionNumber version = update_info.version();

        if (version.isNull())
        {
            ui->label_available->setText(tr("Unknown"));
            ui->edit_description->setText(tr("Error retrieving update information."));
        }
        else
        {
            QVersionNumber current_version(ASPIA_VERSION_MAJOR,
                                           ASPIA_VERSION_MINOR,
                                           ASPIA_VERSION_PATCH);
            if (version > current_version)
            {
                ui->label_available->setText(version.toString());
                ui->edit_description->setText(update_info.description());
                ui->label_url->setText(makeUrl(update_info.url()));
                ui->button_update->setEnabled(true);
            }
            else
            {
                ui->label_available->setText(version.toString());
                ui->edit_description->setText(tr("No updates available."));
            }
        }

        update_info_ = update_info;
    });

    connect(update_checher, &UpdateChecker::finished,
            update_checher, &UpdateChecker::deleteLater);

    ui->label_available->setText(tr("Receiving information..."));
    update_checher->checkForUpdates(update_server, package_name);
}

UpdateDialog::UpdateDialog(const UpdateInfo& update_info, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>()),
      update_info_(update_info)
{
    initialize();

    ui->label_available->setText(update_info_.version().toString());
    ui->label_url->setText(makeUrl(update_info_.url()));
    ui->edit_description->setText(update_info_.description());
    ui->button_update->setEnabled(true);
}

UpdateDialog::~UpdateDialog() = default;

void UpdateDialog::onUpdateNow()
{
    if (QMessageBox::question(
        this,
        tr("Confirmation"),
        tr("An update will be downloaded. "
           "After the download is complete, the application will automatically close. "
           "All unsaved data will be lost.<br/>Continue?"),
        QMessageBox::Yes,
        QMessageBox::No) == QMessageBox::Yes)
    {
        QTemporaryFile file(QDir::tempPath() + QLatin1String("/aspia-XXXXXX.msi"));
        if (!file.open())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("An error occurred while installing the update: %1")
                                 .arg(file.errorString()),
                                 QMessageBox::Ok);
        }
        else
        {
            int result = DownloadDialog(update_info_.url(), file, this).exec();

            if (result == DownloadDialog::Accepted)
                file.setAutoRemove(false);

            file.close();

            // If the download is successfully completed, then run the installer.
            if (result == DownloadDialog::Accepted)
            {
                QString file_name(file.fileName());
                file_name.replace(QLatin1Char('/'), QLatin1Char('\\'));

                QStringList arguments;

                // Normal install.
                arguments << QLatin1String("/i");

                // MSI package file.
                arguments << file_name;

                // Basic UI with no modal dialog boxes.
                arguments << QLatin1String("/qb-!");

                if (executeProcess(QLatin1String("msiexec"),
                                   arguments,
                                   ProcessExecuteMode::ELEVATE))
                {
                    // If the process is successfully launched, then the application is terminated.
                    QCoreApplication::quit();
                }
                else
                {
                    // If the update fails, delete the temporary file.
                    QFile::remove(file.fileName());
                }
            }
        }
    }
}

void UpdateDialog::initialize()
{
    ui->setupUi(this);

    connect(ui->button_update, &QPushButton::released, this, &UpdateDialog::onUpdateNow);
    connect(ui->button_close, &QPushButton::released, this, &UpdateDialog::close);

    QVersionNumber current_version(ASPIA_VERSION_MAJOR,
                                   ASPIA_VERSION_MINOR,
                                   ASPIA_VERSION_PATCH);

    ui->label_current->setText(current_version.toString());
}

} // namespace aspia
