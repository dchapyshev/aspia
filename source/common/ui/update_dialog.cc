//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/update_dialog.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "build/build_config.h"
#include "common/ui/download_dialog.h"
#include "ui_update_dialog.h"

#if defined(OS_WIN)
#include "base/win/process_util.h"
#endif // defined(OS_WIN)

#include <QCloseEvent>
#include <QDir>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QTimer>

namespace common {

namespace {

//--------------------------------------------------------------------------------------------------
QString makeUrl(const QUrl& url)
{
    return QString("<a href='%1'>%1</a>").arg(url.toString());
}

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateDialog::UpdateDialog(const QString& update_server,
                           const QString& package_name,
                           QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>())
{
    LOG(LS_INFO) << "Ctor";
    initialize();

    ui->label_available->setText(tr("Receiving information..."));

    checker_ = std::make_unique<UpdateChecker>();

    checker_->setUpdateServer(update_server);
    checker_->setPackageName(package_name);

    connect(checker_.get(), &UpdateChecker::sig_checkedFinished,
            this, &UpdateDialog::onUpdateCheckedFinished);

    checker_->start();
}

//--------------------------------------------------------------------------------------------------
UpdateDialog::UpdateDialog(const UpdateInfo& update_info, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>()),
      update_info_(update_info)
{
    LOG(LS_INFO) << "Ctor";
    initialize();

    ui->label_available->setText(update_info_.version().toString());
    ui->label_url->setText(makeUrl(update_info_.url()));
    ui->edit_description->setText(update_info_.description());
    ui->button_update->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
UpdateDialog::~UpdateDialog()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        LOG(LS_INFO) << "[ACTION] Escape key pressed";
        close();
        return;
    }

    QDialog::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::closeEvent(QCloseEvent* event)
{
    LOG(LS_INFO) << "Close event";

    if (checker_)
    {
        LOG(LS_INFO) << "Distroy checker...";
        ui->label_available->setText(tr("Cancel checking for updates. Please wait."));
        ui->button_close->setEnabled(false);
        checker_.reset();
    }

    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateNow()
{
    LOG(LS_INFO) << "[ACTION] Update now";

#if defined(OS_WIN)
    QString message1 = tr("An update will be downloaded. After the download is complete, the "
                          "application will automatically close.");
    QString message2 = tr("All connected sessions will be terminated. You cannot establish a "
                          "connection until the update is complete.");
    QString message3 = tr("All unsaved data will be lost.");
    QString question = tr("Continue?");

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            QString("%1<br/><b>%2</b><br/><b>%3</b><br/>%4")
                                .arg(message1, message2, message3, question),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(LS_INFO) << "[ACTION] Update confirmed by user";

        QTemporaryFile file(QDir::tempPath() + QLatin1String("/aspia-XXXXXX.msi"));
        if (!file.open())
        {
            LOG(LS_ERROR) << "Unable to open file: " << file.errorString().toStdString();
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
                QString file_name = QDir::toNativeSeparators(file.fileName());
                QString arguments;

                // Normal install.
                arguments += "/i ";

                // MSI package file.
                arguments += file_name;

                // Basic UI with no modal dialog boxes.
                arguments += " /qb-!";

                if (base::createProcess("msiexec", arguments, base::ProcessExecuteMode::ELEVATE))
                {
                    LOG(LS_INFO) << "msiexec is started";
                    // If the process is successfully launched, then the application is terminated.
                    QCoreApplication::quit();
                }
                else
                {
                    LOG(LS_ERROR) << "Unable to start msiexec process";

                    // If the update fails, delete the temporary file.
                    QString file_name = file.fileName();
                    if (!QFile::remove(file_name))
                    {
                        LOG(LS_ERROR) << "Unable to remove file: " << file_name.toStdString();
                    }
                }
            }
        }
    }
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(LS_ERROR) << "Error while retrieving update information";

        ui->label_available->setText(tr("Unknown"));
        ui->edit_description->setText(tr("Error retrieving update information."));
    }
    else
    {
        update_info_ = UpdateInfo::fromXml(result);
        if (!update_info_.isValid())
        {
            LOG(LS_INFO) << "No updates available";

            ui->label_available->setText(base::kCurrentVersion.toString());
            ui->edit_description->setText(tr("No updates available."));
        }
        else
        {
            const QVersionNumber& update_version = update_info_.version();

            if (update_version > base::kCurrentVersion)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();

                ui->label_available->setText(update_version.toString());
                ui->edit_description->setText(update_info_.description());
                ui->label_url->setText(makeUrl(update_info_.url()));

#if defined(OS_WIN)
                ui->button_update->setEnabled(true);
#endif // defined(OS_WIN)
            }
            else
            {
                LOG(LS_INFO) << "New version less then current: " << update_version.toString();

                ui->label_available->setText(base::kCurrentVersion.toString());
                ui->edit_description->setText(tr("No updates available."));
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        LOG(LS_INFO) << "Destroy update checker";
        checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::initialize()
{
    ui->setupUi(this);

    connect(ui->button_update, &QPushButton::clicked, this, &UpdateDialog::onUpdateNow);
    connect(ui->button_close, &QPushButton::clicked, this, &UpdateDialog::close);

    ui->label_current->setText(base::kCurrentVersion.toString());
}

} // namespace common
