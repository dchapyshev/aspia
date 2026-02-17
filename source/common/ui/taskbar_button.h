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

#ifndef COMMON_UI_TASKBAR_BUTTON_H
#define COMMON_UI_TASKBAR_BUTTON_H

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QWindow>

#include <wrl/client.h>
#include <Shobjidl.h>

namespace common {

class TaskbarProgress;

class TaskbarButton final : public QObject
{
    Q_OBJECT

public:
    explicit TaskbarButton(QObject* parent = nullptr);
    ~TaskbarButton() final = default;

    void setWindow(QWindow* window);
    QWindow* window() const { return window_; }

    QIcon overlayIcon() const { return overlay_icon_; }
    QString overlayDescription() const { return overlay_description_; }

    TaskbarProgress* progress() const;

public slots:
    void setOverlayIcon(const QIcon& icon);
    void setOverlayDescription(const QString& description);
    void clearOverlayIcon();

protected:
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
    void onUpdateProgress();

private:
    void updateOverlayIcon();

    Microsoft::WRL::ComPtr<ITaskbarList4> tblist_;
    QWindow* window_ = nullptr;

    QPointer<TaskbarProgress> progressbar_;
    QIcon overlay_icon_;
    QString overlay_description_;

    Q_DISABLE_COPY(TaskbarButton)
};

} // namespace common

#endif // COMMON_UI_TASKBAR_BUTTON_H
