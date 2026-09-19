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

#ifndef COMMON_ANDROID_UPDATE_WIDGET_H
#define COMMON_ANDROID_UPDATE_WIDGET_H

#include "base/scoped_qpointer.h"
#include "common/update_info.h"
#include "common/android/scroll_area.h"

class QProgressBar;

class Button;
class HttpFileDownloader;
class Label;
class UpdateChecker;
class UpdateInstaller;

class UpdateWidget final : public ScrollArea
{
    Q_OBJECT

public:
    // |package| is the name this application goes by in the manifest of a release.
    explicit UpdateWidget(const QString& package, QWidget* parent = nullptr);
    ~UpdateWidget() final;

    void check(const QString& channel);

private slots:
    void onCheckFinished(const UpdateInfo& update_info);
    void onCheckFailed();
    void onDownloadProgress(int percentage);
    void onDownloadError(const QString& error);
    void onDownloadCompleted();

private:
    void onUpdateClicked();
    void startDownload();
    void setDescription(const QString& text);

    const QString package_;

    Label* label_status_ = nullptr;
    Label* label_description_ = nullptr;
    QProgressBar* progress_ = nullptr;
    Button* button_update_ = nullptr;

    QString channel_;
    UpdateInfo update_info_;

    ScopedQPointer<UpdateChecker> checker_;
    ScopedQPointer<HttpFileDownloader> downloader_;
    ScopedQPointer<UpdateInstaller> installer_;

    Q_DISABLE_COPY_MOVE(UpdateWidget)
};

#endif // COMMON_ANDROID_UPDATE_WIDGET_H
