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

#include "client/ui/desktop_window.h"

#include <QDebug>
#include <QBrush>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>

#include "base/clipboard.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/ui/desktop_panel.h"
#include "client/ui/desktop_widget.h"
#include "desktop_capture/desktop_frame_qimage.h"

namespace aspia {

DesktopWindow::DesktopWindow(ConnectData* connect_data, QWidget* parent)
    : QWidget(parent),
      connect_data_(connect_data)
{
    QString session_name;
    if (connect_data_->sessionType() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        session_name = tr("Aspia Desktop Manage");
    }
    else
    {
        Q_ASSERT(connect_data_->sessionType() == proto::auth::SESSION_TYPE_DESKTOP_VIEW);
        session_name = tr("Aspia Desktop View");
    }

    QString computer_name;
    if (!connect_data_->computerName().isEmpty())
        computer_name = connect_data_->computerName();
    else
        computer_name = connect_data_->address();

    setWindowTitle(QString("%1 - %2").arg(computer_name).arg(session_name));
    setMinimumSize(800, 600);

    desktop_ = new DesktopWidget(this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setAutoFillBackground(true);
    scroll_area_->setWidget(desktop_);

    QPalette palette(scroll_area_->palette());
    palette.setBrush(QPalette::Background, QBrush(QColor(25, 25, 25)));
    scroll_area_->setPalette(palette);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    panel_ = new DesktopPanel(connect_data_->sessionType(), this);

    connect(panel_, &DesktopPanel::keySequence, desktop_, &DesktopWidget::executeKeySequense);
    connect(panel_, &DesktopPanel::settingsButton, this, &DesktopWindow::changeSettings);
    connect(panel_, &DesktopPanel::switchToAutosize, this, &DesktopWindow::autosizeWindow);
    connect(panel_, &DesktopPanel::screenSelected, this, &DesktopWindow::sendScreen);

    connect(panel_, &DesktopPanel::switchToFullscreen, [this](bool fullscreen)
    {
        if (fullscreen)
        {
            is_maximized_ = isMaximized();
            showFullScreen();
        }
        else
        {
            if (is_maximized_)
                showMaximized();
            else
                showNormal();
        }
    });

    connect(panel_, &DesktopPanel::autoScrollChanged, [this](bool enabled)
    {
        autoscroll_enabled_ = enabled;
    });

    connect(desktop_, &DesktopWidget::sendPointerEvent, this, &DesktopWindow::onPointerEvent);
    connect(desktop_, &DesktopWidget::sendKeyEvent, this, &DesktopWindow::sendKeyEvent);

    desktop_->installEventFilter(this);
    scroll_area_->viewport()->installEventFilter(this);

    clipboard_ = new Clipboard(this);
    connect(clipboard_, &Clipboard::clipboardEvent, this, &DesktopWindow::sendClipboardEvent);
}

void DesktopWindow::resizeDesktopFrame(const DesktopRect& screen_rect)
{
    DesktopSize prev_size = DesktopSize::fromQSize(desktop_->size());

    desktop_->resizeDesktopFrame(screen_rect.size());

    if (screen_rect.size() != prev_size && !isMaximized() && !isFullScreen())
        autosizeWindow();

    screen_top_left_ = screen_rect.leftTop();
}

void DesktopWindow::drawDesktopFrame()
{
    desktop_->update();
    panel_->update();
}

DesktopFrame* DesktopWindow::desktopFrame()
{
    return desktop_->desktopFrame();
}

void DesktopWindow::injectCursor(const QCursor& cursor)
{
    desktop_->setCursor(cursor);
}

void DesktopWindow::injectClipboard(const proto::desktop::ClipboardEvent& event)
{
    if (!clipboard_.isNull())
        clipboard_->injectClipboardEvent(event);
}

void DesktopWindow::setScreenList(const proto::desktop::ScreenList& screen_list)
{
    panel_->setScreenList(screen_list);
}

void DesktopWindow::onPointerEvent(const QPoint& pos, uint32_t mask)
{
    if (autoscroll_enabled_)
    {
        QPoint cursor = desktop_->mapTo(scroll_area_, pos);
        QRect client_area = scroll_area_->rect();

        QScrollBar* hscrollbar = scroll_area_->horizontalScrollBar();
        QScrollBar* vscrollbar = scroll_area_->verticalScrollBar();

        if (!hscrollbar->isHidden())
            client_area.setHeight(client_area.height() - hscrollbar->height());

        if (!vscrollbar->isHidden())
            client_area.setWidth(client_area.width() - vscrollbar->width());

        scroll_delta_.setX(0);
        scroll_delta_.setY(0);

        if (client_area.width() < desktop_->width())
        {
            if (cursor.x() > client_area.width() - 50)
                scroll_delta_.setX(10);
            else if (cursor.x() < 50)
                scroll_delta_.setX(-10);
        }

        if (client_area.height() < desktop_->height())
        {
            if (cursor.y() > client_area.height() - 50)
                scroll_delta_.setY(10);
            else if (cursor.y() < 50)
                scroll_delta_.setY(-10);
        }

        if (!scroll_delta_.isNull())
        {
            if (scroll_timer_id_ == 0)
                scroll_timer_id_ = startTimer(15);
        }
        else if (scroll_timer_id_ != 0)
        {
            killTimer(scroll_timer_id_);
            scroll_timer_id_ = 0;
        }
    }
    else if (scroll_timer_id_ != 0)
    {
        killTimer(scroll_timer_id_);
        scroll_timer_id_ = 0;
    }

    emit sendPointerEvent(DesktopPoint::fromQPoint(pos).translated(screen_top_left_), mask);
}

void DesktopWindow::changeSettings()
{
    QScopedPointer<DesktopConfigDialog> dialog(
        new DesktopConfigDialog(connect_data_->sessionType(),
                                connect_data_->desktopConfig(),
                                this));

    connect(dialog.get(), &DesktopConfigDialog::configChanged,
            this, &DesktopWindow::onConfigChanged);

    dialog->exec();
}

void DesktopWindow::onConfigChanged(const proto::desktop::Config& config)
{
    connect_data_->setDesktopConfig(config);
    emit sendConfig(config);
}

void DesktopWindow::autosizeWindow()
{
    QRect screen_rect = QApplication::desktop()->availableGeometry(this);
    QSize window_size = desktop_->size() + frameSize() - size();

    if (window_size.width() < screen_rect.width() && window_size.height() < screen_rect.height())
    {
        showNormal();

        resize(desktop_->size());
        move(screen_rect.x() + (screen_rect.width() / 2 - window_size.width() / 2),
             screen_rect.y() + (screen_rect.height() / 2 - window_size.height() / 2));
    }
    else
    {
        showMaximized();
    }
}

void DesktopWindow::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == scroll_timer_id_)
    {
        if (scroll_delta_.x() != 0)
        {
            QScrollBar* scrollbar = scroll_area_->horizontalScrollBar();

            int pos = scrollbar->sliderPosition() + scroll_delta_.x();

            pos = qMax(pos, scrollbar->minimum());
            pos = qMin(pos, scrollbar->maximum());

            scrollbar->setSliderPosition(pos);
        }

        if (scroll_delta_.y() != 0)
        {
            QScrollBar* scrollbar = scroll_area_->verticalScrollBar();

            int pos = scrollbar->sliderPosition() + scroll_delta_.y();

            pos = qMax(pos, scrollbar->minimum());
            pos = qMin(pos, scrollbar->maximum());

            scrollbar->setSliderPosition(pos);
        }
    }

    QWidget::timerEvent(event);
}

void DesktopWindow::resizeEvent(QResizeEvent* event)
{
    panel_->move(QPoint(width() / 2 - panel_->width() / 2, 0));
    QWidget::resizeEvent(event);
}

void DesktopWindow::closeEvent(QCloseEvent* event)
{
    for (const auto& object : children())
    {
        QWidget* widget = dynamic_cast<QWidget*>(object);
        if (widget)
            widget->close();
    }

    emit windowClose();
    QWidget::closeEvent(event);
}

void DesktopWindow::leaveEvent(QEvent* event)
{
    if (scroll_timer_id_)
    {
        killTimer(scroll_timer_id_);
        scroll_timer_id_ = 0;
    }

    QWidget::leaveEvent(event);
}

bool DesktopWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == desktop_)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent* key_event = dynamic_cast<QKeyEvent*>(event);
            if (key_event && key_event->key() == Qt::Key_Tab)
            {
                desktop_->doKeyEvent(key_event);
                return true;
            }
        }

        return false;
    }
    else if (object == scroll_area_->viewport())
    {
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent* wheel_event = dynamic_cast<QWheelEvent*>(event);
            if (wheel_event)
            {
                QPoint pos = desktop_->mapFromGlobal(wheel_event->globalPos());

                desktop_->doMouseEvent(wheel_event->type(),
                                       wheel_event->buttons(),
                                       pos,
                                       wheel_event->angleDelta());
                return true;
            }
        }
    }

    return QWidget::eventFilter(object, event);
}

} // namespace aspia
