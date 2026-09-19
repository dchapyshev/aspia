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

#include "common/android/update_widget.h"

#include <QProgressBar>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/version_constants.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "common/update_installer.h"
#include "common/android/button.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateWidget::UpdateWidget(const QString& package, QWidget* parent)
    : ScrollArea(parent),
      package_(package)
{
    LOG(INFO) << "Ctor";

    label_status_ = new Label(QString(), Label::Role::BODY);
    label_status_->setWordWrap(true);

    label_description_ = new Label(QString(), Label::Role::CAPTION);
    label_description_->setWordWrap(true);
    label_description_->setVisible(false);

    progress_ = new QProgressBar();
    progress_->setRange(0, 100);
    progress_->setTextVisible(false);
    progress_->setVisible(false);

    button_update_ = new Button(tr("Update"), Button::Role::FILLED);
    button_update_->setEnabled(false);

    connect(button_update_, &Button::clicked, this, &UpdateWidget::onUpdateClicked);

    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);
    layout->addWidget(new Label(tr("Installed version: %1").arg(kCurrentVersion.toString()),
                                Label::Role::CAPTION));
    layout->addWidget(label_status_);
    layout->addWidget(label_description_);
    layout->addWidget(progress_);
    layout->addWidget(button_update_);
    layout->addStretch();

    setWidget(content);
}

//--------------------------------------------------------------------------------------------------
UpdateWidget::~UpdateWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::check(const QString& channel)
{
    channel_ = channel;
    update_info_ = UpdateInfo();

    // A deferred deletion leaves the object alive and connected for a while longer, so a thread
    // that is still running would deliver what it finds into the check that replaced it.
    if (checker_)
    {
        checker_->disconnect(this);
        checker_.reset();
    }

    if (downloader_)
    {
        downloader_->disconnect(this);
        downloader_.reset();
    }

    installer_.reset();

    label_status_->setText(tr("Receiving information..."));
    setDescription(QString());
    progress_->setVisible(false);
    button_update_->setEnabled(false);

    checker_ = new UpdateChecker(channel_, package_, this);

    connect(checker_, &UpdateChecker::sig_checkFinished, this, &UpdateWidget::onCheckFinished);
    connect(checker_, &UpdateChecker::sig_checkFailed, this, &UpdateWidget::onCheckFailed);

    checker_->start();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onCheckFinished(const UpdateInfo& update_info)
{
    update_info_ = update_info;

    if (!update_info_.isValid())
    {
        LOG(INFO) << "No updates available";
        label_status_->setText(tr("No updates available."));
    }
    else
    {
        LOG(INFO) << "New version available:" << update_info_.version().toString();

        label_status_->setText(tr("Version %1 is available.").arg(update_info_.version().toString()));

        setDescription(update_info_.description());
        button_update_->setEnabled(true);
    }

    checker_->disconnect(this);
    checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onCheckFailed()
{
    LOG(ERROR) << "Error while retrieving update information";
    label_status_->setText(tr("Error retrieving update information."));

    checker_->disconnect(this);
    checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadProgress(int percentage)
{
    progress_->setValue(percentage);
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadError(const QString& error)
{
    LOG(ERROR) << "Unable to download update:" << error;

    downloader_->disconnect(this);
    downloader_.reset();
    installer_.reset();

    progress_->setVisible(false);
    button_update_->setEnabled(true);

    label_status_->setText(tr("An error occurred while downloading the update: %1").arg(error));
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadCompleted()
{
    downloader_->disconnect(this);
    downloader_.reset();

    progress_->setVisible(false);

    UpdateInstaller::Result result = installer_->install();
    installer_.reset();

    if (result == UpdateInstaller::Result::STARTED)
    {
        // The installer of the system is in front now and asks for the rest by itself.
        label_status_->setText(tr("Installing the update. Please wait."));
        return;
    }

    button_update_->setEnabled(true);

    if (result == UpdateInstaller::Result::DAMAGED)
        label_status_->setText(tr("The downloaded file is damaged."));
    else
        label_status_->setText(tr("An error occurred while installing the update."));
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onUpdateClicked()
{
    LOG(INFO) << "[ACTION] Update now";

    if (!UpdateInstaller::isSupported(update_info_.format()))
    {
        MessageDialog::info(this, tr("Update"),
                            tr("Install the update yourself: %1").arg(update_info_.url()));
        return;
    }

    // A package is installed only from an application the user has allowed to ask for it, and that
    // is allowed in the settings of the system.
    if (!UpdateInstaller::canInstall())
    {
        if (MessageDialog::confirm(this, tr("Update"),
                tr("To install the update, allow this application to install unknown apps on the "
                   "next screen."), tr("Allow")))
        {
            UpdateInstaller::openInstallPermission();
        }

        return;
    }

    if (!MessageDialog::confirm(this, tr("Update"),
            tr("An update will be downloaded. All connected sessions will be terminated and all "
               "unsaved data will be lost."), tr("Continue")))
    {
        return;
    }

    LOG(INFO) << "[ACTION] Update confirmed by user";
    startDownload();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::startDownload()
{
    installer_ = new UpdateInstaller(UpdateInstaller::Mode::USER, this);

    QString file_path = installer_->createPackageFile(update_info_);
    if (file_path.isEmpty())
    {
        installer_.reset();
        label_status_->setText(tr("An error occurred while installing the update."));
        return;
    }

    downloader_ = new HttpFileDownloader(update_info_.url(), file_path, this);

    connect(downloader_, &HttpFileDownloader::sig_downloadProgress,
            this, &UpdateWidget::onDownloadProgress);
    connect(downloader_, &HttpFileDownloader::sig_downloadError,
            this, &UpdateWidget::onDownloadError);
    connect(downloader_, &HttpFileDownloader::sig_downloadCompleted,
            this, &UpdateWidget::onDownloadCompleted);

    button_update_->setEnabled(false);
    progress_->setValue(0);
    progress_->setVisible(true);

    label_status_->setText(tr("Downloading the update. Please wait."));

    downloader_->start();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::setDescription(const QString& text)
{
    label_description_->setText(text);
    label_description_->setVisible(!text.isEmpty());
}
