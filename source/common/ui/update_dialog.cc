//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "build/build_config.h"
#include "common/ui/download_dialog.h"
#include "qt_base/application.h"
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
UpdateDialog::UpdateDialog(std::string_view update_server,
                           std::string_view package_name,
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

    checker_->start(qt_base::Application::uiTaskRunner(), this);
}

//--------------------------------------------------------------------------------------------------
UpdateDialog::UpdateDialog(const UpdateInfo& update_info, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::UpdateDialog>()),
      update_info_(update_info)
{
    initialize();

    ui->label_available->setText(QString::fromStdString(update_info_.version().toString(3)));
    ui->label_url->setText(makeUrl(QString::fromStdString(update_info_.url())));
    ui->edit_description->setText(QString::fromStdString(update_info_.description()));
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
        close();
        return;
    }

    QDialog::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::closeEvent(QCloseEvent* event)
{
    if (checker_)
    {
        ui->label_available->setText(tr("Cancel checking for updates. Please wait."));
        ui->button_close->setEnabled(false);
        checker_.reset();
    }

    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateCheckedFinished(const base::ByteArray& result)
{
    if (result.empty())
    {
        LOG(LS_ERROR) << "Error while retrieving update information";

        ui->label_available->setText(tr("Unknown"));
        ui->edit_description->setText(tr("Error retrieving update information."));
    }
    else
    {
        const base::Version& current_version = base::Version::kVersion_CurrentShort;

        update_info_ = UpdateInfo::fromXml(result);
        if (!update_info_.isValid())
        {
            LOG(LS_INFO) << "No updates available";

            ui->label_available->setText(QString::fromStdString(current_version.toString(3)));
            ui->edit_description->setText(tr("No updates available."));
        }
        else
        {
            const base::Version& update_version = update_info_.version();

            if (update_version > current_version)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();

                ui->label_available->setText(QString::fromStdString(update_version.toString(3)));
                ui->edit_description->setText(QString::fromStdString(update_info_.description()));
                ui->label_url->setText(makeUrl(QString::fromStdString(update_info_.url())));

#if defined(OS_WIN)
                ui->button_update->setEnabled(true);
#endif // defined(OS_WIN)
            }
            else
            {
                LOG(LS_INFO) << "New version less then current: " << update_version.toString();

                ui->label_available->setText(QString::fromStdString(current_version.toString(3)));
                ui->edit_description->setText(tr("No updates available."));
            }
        }
    }

    QTimer::singleShot(0, this, [this]()
    {
        checker_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void UpdateDialog::onUpdateNow()
{
    LOG(LS_INFO) << "ACTION: Update now";

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
                                .arg(message1).arg(message2).arg(message3).arg(question),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
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

                std::u16string arguments;

                // Normal install.
                arguments += u"/i ";

                // MSI package file.
                arguments += file_name.toStdU16String();

                // Basic UI with no modal dialog boxes.
                arguments += u" /qb-!";

                if (base::win::createProcess(u"msiexec",
                                              arguments,
                                              base::win::ProcessExecuteMode::ELEVATE))
                {
                    // If the process is successfully launched, then the application is terminated.
                    QCoreApplication::quit();
                }
                else
                {
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
void UpdateDialog::initialize()
{
    ui->setupUi(this);

    connect(ui->button_update, &QPushButton::clicked, this, &UpdateDialog::onUpdateNow);
    connect(ui->button_close, &QPushButton::clicked, this, &UpdateDialog::close);

    const base::Version& current_version = base::Version::kVersion_CurrentShort;

    ui->label_current->setText(QString::fromStdString(current_version.toString(3)));
}

} // namespace common
