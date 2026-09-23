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

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/net/http_file_downloader.h"
#include "base/update/update_checker.h"
#include "base/update/update_installer.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/message_dialog.h"

namespace {

constexpr int kContentMargin = 16;
constexpr int kRowSpacing = 16;
constexpr int kHeaderSpacing = 16;
constexpr int kAppIconSize = 56;
constexpr int kDescriptionSpacing = 4;

//--------------------------------------------------------------------------------------------------
QString titleText(const QVersionNumber& version)
{
    return QString("Aspia %1").arg(version.toString());
}

} // namespace

//--------------------------------------------------------------------------------------------------
UpdateWidget::UpdateWidget(const QString& package, QWidget* parent)
    : ScrollArea(parent),
      package_(package)
{
    LOG(INFO) << "Ctor";

    // The package of the update installed before this run is still in the private directory of the
    // application, and nothing is reading it any more.
    UpdateInstaller::removeLeftovers();

    QLabel* app_icon = new QLabel();
    app_icon->setPixmap(GuiApplication::svgPixmap(":/img/aspia.svg", QSize(kAppIconSize, kAppIconSize)));

    label_title_ = new Label(titleText(kCurrentVersion), Label::Role::TITLE);

    label_status_ = new Label(QString(), Label::Role::CAPTION);
    label_status_->setWordWrap(true);

    label_error_ = new Label(QString(), Label::Role::CAPTION);
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    button_update_ = new Button(tr("Update"), Button::Role::FILLED);
    button_update_->setVisible(false);

    button_cancel_ = new IconButton(":/img/material/close.svg");
    button_cancel_->setVisible(false);

    button_check_ = new Button(tr("Check again"), Button::Role::OUTLINED);
    button_check_->setVisible(false);

    label_whats_new_ = new Label(tr("What's new"), Label::Role::CAPTION);
    label_whats_new_->setVisible(false);

    label_description_ = new Label(QString(), Label::Role::CAPTION);
    label_description_->setWordWrap(true);
    label_description_->setVisible(false);

    connect(button_update_, &Button::clicked, this, &UpdateWidget::onUpdateClicked);
    connect(button_cancel_, &IconButton::clicked, this, &UpdateWidget::cancelDownload);
    connect(button_check_, &Button::clicked, this, &UpdateWidget::onCheckClicked);

    QVBoxLayout* title_layout = new QVBoxLayout();
    title_layout->setSpacing(0);
    title_layout->addWidget(label_title_);
    title_layout->addWidget(label_status_);

    QHBoxLayout* header_layout = new QHBoxLayout();
    header_layout->setSpacing(kHeaderSpacing);
    header_layout->addWidget(app_icon);
    header_layout->addLayout(title_layout, 1);

    QHBoxLayout* action_layout = new QHBoxLayout();
    action_layout->addWidget(button_update_, 1);
    action_layout->addWidget(button_cancel_);

    QVBoxLayout* description_layout = new QVBoxLayout();
    description_layout->setSpacing(kDescriptionSpacing);
    description_layout->addWidget(label_whats_new_);
    description_layout->addWidget(label_description_);

    QWidget* content = new QWidget(this);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->setSpacing(kRowSpacing);
    layout->addLayout(header_layout);
    layout->addWidget(label_error_);
    layout->addLayout(action_layout);
    layout->addWidget(button_check_);
    layout->addLayout(description_layout);
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
    // The screen is left and entered again while the update it started is downloading, and a check
    // over it would take away the package the installation is waiting for.
    if (downloader_)
        return;

    channel_ = channel;
    update_info_ = UpdateInfo();

    // A deferred deletion leaves the object alive and connected for a while longer, so a thread
    // that is still running would deliver what it finds into the check that replaced it.
    if (checker_)
    {
        checker_->disconnect(this);
        checker_.reset();
    }

    installer_.reset();

    label_title_->setText(titleText(kCurrentVersion));
    label_status_->setText(tr("Receiving information..."));
    setError(QString());
    setDescription(QString());
    button_update_->setVisible(false);
    button_check_->setVisible(false);

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
        showNotAvailable(tr("No updates available."));
    }
    else
    {
        LOG(INFO) << "New version available:" << update_info_.version().toString();

        showAvailable();
        setDescription(update_info_.description());
    }

    checker_->disconnect(this);
    checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onCheckFailed()
{
    LOG(ERROR) << "Error while retrieving update information";
    showNotAvailable(tr("Installed version: %1").arg(kCurrentVersion.toString()));
    setError(tr("Error retrieving update information."));

    checker_->disconnect(this);
    checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadProgress(int percentage)
{
    // A signal queued by the thread of the downloader before it was disconnected is still delivered.
    if (!downloader_)
        return;

    button_update_->setProgress(percentage);
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadError(const QString& error)
{
    if (!downloader_)
        return;

    LOG(ERROR) << "Unable to download update:" << error;

    downloader_->disconnect(this);
    downloader_.reset();
    installer_.reset();

    showAvailable();
    setError(tr("An error occurred while downloading the update: %1").arg(error));
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::onDownloadCompleted()
{
    if (!downloader_)
        return;

    downloader_->disconnect(this);
    downloader_.reset();

    UpdateInstaller::Result result = installer_->install();
    installer_.reset();

    showAvailable();

    if (result == UpdateInstaller::Result::STARTED)
        return;

    if (result == UpdateInstaller::Result::DAMAGED)
        setError(tr("The downloaded file is damaged."));
    else
        setError(tr("An error occurred while installing the update."));
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
void UpdateWidget::onCheckClicked()
{
    LOG(INFO) << "[ACTION] Check for updates again";
    check(channel_);
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::startDownload()
{
    installer_ = new UpdateInstaller(UpdateInstaller::Mode::USER, this);

    QString file_path = installer_->createPackageFile(update_info_);
    if (file_path.isEmpty())
    {
        installer_.reset();
        setError(tr("An error occurred while installing the update."));
        return;
    }

    downloader_ = new HttpFileDownloader(update_info_.url(), file_path, this);

    connect(downloader_, &HttpFileDownloader::sig_downloadProgress,
            this, &UpdateWidget::onDownloadProgress);
    connect(downloader_, &HttpFileDownloader::sig_downloadError,
            this, &UpdateWidget::onDownloadError);
    connect(downloader_, &HttpFileDownloader::sig_downloadCompleted,
            this, &UpdateWidget::onDownloadCompleted);

    setError(QString());
    label_status_->setText(tr("Downloading the update. Please wait."));
    onDownloadProgress(0);
    button_cancel_->setVisible(true);

    downloader_->start();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::cancelDownload()
{
    if (!downloader_)
        return;

    LOG(INFO) << "[ACTION] Cancel update";

    downloader_->disconnect(this);
    downloader_.reset();
    installer_.reset();

    showAvailable();
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::showAvailable()
{
    label_title_->setText(titleText(update_info_.version()));
    label_status_->setText(tr("Installed version: %1").arg(kCurrentVersion.toString()));

    button_update_->setProgress(-1);
    button_update_->setVisible(true);
    button_cancel_->setVisible(false);
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::showNotAvailable(const QString& status)
{
    label_status_->setText(status);
    button_check_->setVisible(true);
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::setDescription(const QString& text)
{
    label_description_->setText(text);
    label_whats_new_->setVisible(!text.isEmpty());
    label_description_->setVisible(!text.isEmpty());
}

//--------------------------------------------------------------------------------------------------
void UpdateWidget::setError(const QString& text)
{
    label_error_->setText(text);
    label_error_->setVisible(!text.isEmpty());
}
