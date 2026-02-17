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

#include "common/ui/taskbar_button.h"

#include <QAbstractNativeEventFilter>
#include <QEvent>
#include <QGuiApplication>
#include <QImage>

#include <comdef.h>

#include "base/logging.h"
#include "base/win/scoped_user_object.h"
#include "common/ui/taskbar_progress.h"

namespace common {

namespace {

const int TaskbarButtonCreated = QEvent::registerEventType();

class EventFilter final : public QAbstractNativeEventFilter
{
public:
    EventFilter()
        : button_created_message_id_(RegisterWindowMessageW(L"TaskbarButtonCreated"))
    {
        // Nothing
    }

    ~EventFilter() final
    {
        instance = nullptr;
    }

    bool nativeEventFilter(const QByteArray& /* event_type */, void* message, qintptr* result) final
    {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message != button_created_message_id_)
            return false;

        const QWindowList windows = QGuiApplication::topLevelWindows();
        for (QWindow* window : std::as_const(windows))
        {
            if (window->handle() && window->winId() == reinterpret_cast<WId>(msg->hwnd))
            {
                QEvent event(static_cast<QEvent::Type>(TaskbarButtonCreated));
                QCoreApplication::sendEvent(window, &event);
            }
        }

        if (result)
            *result = 0;

        return true;
    }

    static EventFilter* instance;

private:
    UINT button_created_message_id_;
};

//--------------------------------------------------------------------------------------------------
EventFilter* EventFilter::instance = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
TaskbarButton::TaskbarButton(QObject* parent)
    : QObject(parent)
{
    _com_error error = CoCreateInstance(
        CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tblist_));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CoCreateInstance failed:" << error;
        return;
    }

    error = tblist_->HrInit();
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "HrInit failed:" << error;
        return;
    }

    if (!EventFilter::instance)
    {
        EventFilter::instance = new EventFilter();
        qApp->installNativeEventFilter(EventFilter::instance);
    }

    setWindow(qobject_cast<QWindow*>(parent));
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::setWindow(QWindow* window)
{
    if (window_)
        window_->removeEventFilter(this);

    window_ = window;

    if (!window_)
        return;

    window_->installEventFilter(this);
    if (window_->isVisible())
    {
        onUpdateProgress();
        updateOverlayIcon();
    }
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::setOverlayIcon(const QIcon& icon)
{
    overlay_icon_ = icon;
    updateOverlayIcon();
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::clearOverlayIcon()
{
    overlay_description_ = QString();
    overlay_icon_ = QIcon();
    updateOverlayIcon();
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::setOverlayDescription(const QString& description)
{
    overlay_description_ = description;
    updateOverlayIcon();
}

//--------------------------------------------------------------------------------------------------
TaskbarProgress* TaskbarButton::progress() const
{
    if (!progressbar_)
    {
        TaskbarButton* button = const_cast<TaskbarButton*>(this);
        TaskbarProgress* progress = new TaskbarProgress(button);

        connect(progress, &TaskbarProgress::destroyed, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_valueChanged, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_minimumChanged, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_maximumChanged, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_visibilityChanged, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_pausedChanged, this, &TaskbarButton::onUpdateProgress);
        connect(progress, &TaskbarProgress::sig_stoppedChanged, this, &TaskbarButton::onUpdateProgress);

        button->progressbar_ = progress;
        button->onUpdateProgress();
    }

    return progressbar_;
}

//--------------------------------------------------------------------------------------------------
bool TaskbarButton::eventFilter(QObject* object, QEvent* event)
{
    if (object == window_ && event->type() == TaskbarButtonCreated)
    {
        onUpdateProgress();
        updateOverlayIcon();
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::onUpdateProgress()
{
    if (!tblist_ || !window_)
        return;

    HWND hwnd = reinterpret_cast<HWND>(window_->winId());

    if (progressbar_)
    {
        int min = progressbar_->minimum();
        int max = progressbar_->maximum();
        int range = max - min;

        if (range > 0)
        {
            ULONGLONG value =
                qRound(double(100) * (double(progressbar_->value() - min)) / double(range));
            tblist_->SetProgressValue(hwnd, value, 100);
        }
    }

    TBPFLAG flag = TBPF_NORMAL;

    if (!progressbar_ || !progressbar_->isVisible())
        flag = TBPF_NOPROGRESS;
    else if (progressbar_->isStopped())
        flag = TBPF_ERROR;
    else if (progressbar_->isPaused())
        flag = TBPF_PAUSED;
    else if (progressbar_->minimum() == 0 && progressbar_->maximum() == 0)
        flag = TBPF_INDETERMINATE;

    tblist_->SetProgressState(hwnd, flag);
}

//--------------------------------------------------------------------------------------------------
void TaskbarButton::updateOverlayIcon()
{
    if (!tblist_ || !window_)
        return;

    HWND hwnd = reinterpret_cast<HWND>(window_->winId());
    const wchar_t* desc = nullptr;
    base::ScopedHICON icon;

    if (!overlay_description_.isEmpty())
        desc = qUtf16Printable(overlay_description_);

    if (!overlay_icon_.isNull())
        icon.reset(overlay_icon_.pixmap(GetSystemMetrics(SM_CXSMICON)).toImage().toHICON());

    if (icon)
    {
        tblist_->SetOverlayIcon(hwnd, icon, desc);
    }
    else if (!icon && !overlay_icon_.isNull())
    {
        HICON image = static_cast<HICON>(LoadImageW(
            nullptr, IDI_APPLICATION, IMAGE_ICON, SM_CXSMICON, SM_CYSMICON, LR_SHARED));
        tblist_->SetOverlayIcon(hwnd, image, desc);
    }
    else
    {
        tblist_->SetOverlayIcon(hwnd, nullptr, desc);
    }
}

} // namespace common
