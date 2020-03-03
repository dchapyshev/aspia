//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/qt_desktop_window.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "client/client_desktop.h"
#include "client/desktop_control_proxy.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/ui/desktop_panel.h"
#include "client/ui/frame_factory_qimage.h"
#include "client/ui/frame_qimage.h"
#include "client/ui/qt_file_manager_window.h"
#include "client/ui/system_info_window.h"
#include "common/desktop_session_constants.h"
#include "desktop/mouse_cursor.h"

#include <QApplication>
#include <QBrush>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>

namespace client {

namespace {

QSize scaledSize(const QSize& source_size, int scale)
{
    if (scale == -1)
        return source_size;

    return QSize((source_size.width() * scale) / 100,
                 (source_size.height() * scale) / 100);
}

} // namespace

QtDesktopWindow::QtDesktopWindow(proto::SessionType session_type,
                                 const proto::DesktopConfig& desktop_config,
                                 QWidget* parent)
    : ClientWindow(parent),
      session_type_(session_type),
      desktop_config_(desktop_config)
{
    setMinimumSize(400, 300);

    desktop_ = new DesktopWidget(this, this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setAutoFillBackground(true);
    scroll_area_->setWidget(desktop_);

    QPalette palette(scroll_area_->palette());
    palette.setBrush(QPalette::Window, QBrush(QColor(25, 25, 25)));
    scroll_area_->setPalette(palette);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    panel_ = new DesktopPanel(session_type_, this);

    desktop_->enableKeyCombinations(panel_->sendKeyCombinations());

    connect(panel_, &DesktopPanel::keyCombination, desktop_, &DesktopWidget::executeKeyCombination);
    connect(panel_, &DesktopPanel::settingsButton, this, &QtDesktopWindow::changeSettings);
    connect(panel_, &DesktopPanel::switchToAutosize, this, &QtDesktopWindow::autosizeWindow);
    connect(panel_, &DesktopPanel::takeScreenshot, this, &QtDesktopWindow::takeScreenshot);
    connect(panel_, &DesktopPanel::scaleChanged, this, &QtDesktopWindow::scaleDesktop);
    connect(panel_, &DesktopPanel::closeSession, this, &QtDesktopWindow::close);

    connect(panel_, &DesktopPanel::screenSelected, [this](const proto::Screen& screen)
    {
        desktop_control_proxy_->setCurrentScreen(screen);
    });

    connect(panel_, &DesktopPanel::powerControl, [this](proto::PowerControl::Action action)
    {
        desktop_control_proxy_->onPowerControl(action);
    });

    connect(panel_, &DesktopPanel::startRemoteUpdate, [this]()
    {
        desktop_control_proxy_->onRemoteUpdate();
    });

    connect(panel_, &DesktopPanel::startSystemInfo, [this]()
    {
        desktop_control_proxy_->onSystemInfoRequest();
    });

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

    connect(panel_, &DesktopPanel::keyCombinationsChanged,
            desktop_, &DesktopWidget::enableKeyCombinations);

    desktop_->installEventFilter(this);
    scroll_area_->viewport()->installEventFilter(this);

    connect(panel_, &DesktopPanel::startSession, [this](proto::SessionType session_type)
    {
        client::Config session_config = config();
        session_config.session_type = session_type;

        client::ClientWindow* client_window = nullptr;

        switch (session_config.session_type)
        {
            case proto::SESSION_TYPE_FILE_TRANSFER:
                client_window = new client::QtFileManagerWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!client_window)
            return;

        client_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!client_window->connectToHost(session_config))
            client_window->close();
    });
}

QtDesktopWindow::~QtDesktopWindow() = default;

std::unique_ptr<Client> QtDesktopWindow::createClient(
    std::shared_ptr<base::TaskRunner> ui_task_runner)
{
    std::unique_ptr<ClientDesktop> client =
        std::make_unique<ClientDesktop>(std::move(ui_task_runner));

    client->setDesktopConfig(desktop_config_);
    client->setDesktopWindow(this);

    return client;
}

void QtDesktopWindow::showWindow(
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy, const base::Version& peer_version)
{
    desktop_control_proxy_ = std::move(desktop_control_proxy);
    peer_version_ = peer_version;

    clipboard_ = std::make_unique<common::Clipboard>();
    clipboard_->start(this);

    show();
    activateWindow();
}

void QtDesktopWindow::configRequired()
{
    if (!(video_encodings_ & common::kSupportedVideoEncodings))
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("There are no supported video encodings."),
                             QMessageBox::Ok);
        close();
    }
    else
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("The current video encoding is not supported by the host. "
                                "Please specify a different video encoding."),
                             QMessageBox::Ok);

        DesktopConfigDialog* dialog = new DesktopConfigDialog(
            session_type_, desktop_config_, video_encodings_, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(dialog, &DesktopConfigDialog::configChanged, this, &QtDesktopWindow::onConfigChanged);
        connect(dialog, &DesktopConfigDialog::rejected, this, &QtDesktopWindow::close);

        dialog->show();
        dialog->activateWindow();
    }
}

void QtDesktopWindow::setCapabilities(const std::string& extensions, uint32_t video_encodings)
{
    video_encodings_ = video_encodings;

    // The list of extensions is passed as a string. Extensions are separated by a semicolon.
    std::vector<std::string_view> extensions_list = base::splitStringView(
        extensions, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // By default, remote update is disabled.
    panel_->enableRemoteUpdate(false);

    if (base::contains(extensions_list, common::kRemoteUpdateExtension))
    {
        if (Client::version() > peer_version_)
            panel_->enableRemoteUpdate(true);
    }

    panel_->enablePowerControl(base::contains(extensions_list, common::kPowerControlExtension));
    panel_->enableScreenSelect(base::contains(extensions_list, common::kSelectScreenExtension));
    panel_->enableSystemInfo(base::contains(extensions_list, common::kSystemInfoExtension));
}

void QtDesktopWindow::setScreenList(const proto::ScreenList& screen_list)
{
    panel_->setScreenList(screen_list);
}

void QtDesktopWindow::setSystemInfo(const proto::SystemInfo& system_info)
{
    if (!system_info_)
    {
        system_info_ = new SystemInfoWindow(this);
        system_info_->setAttribute(Qt::WA_DeleteOnClose);

        connect(system_info_, &SystemInfoWindow::systemInfoRequired, [this]()
        {
            desktop_control_proxy_->onSystemInfoRequest();
        });
    }

    system_info_->setSystemInfo(system_info);
    system_info_->show();
    system_info_->activateWindow();
}

std::unique_ptr<FrameFactory> QtDesktopWindow::frameFactory()
{
    return std::make_unique<FrameFactoryQImage>();
}

void QtDesktopWindow::drawFrame(std::shared_ptr<desktop::Frame> frame)
{
    desktop::Frame* current_frame = desktop_->desktopFrame();

    if (current_frame != frame.get())
    {
        screen_top_left_ = frame->topLeft();

        desktop_->setDesktopFrame(frame);

        scaleDesktop();

        if (!current_frame)
            autosizeWindow();
    }

    desktop_->update();
}

void QtDesktopWindow::drawMouseCursor(std::shared_ptr<desktop::MouseCursor> mouse_cursor)
{
    QImage image(mouse_cursor->data(),
                 mouse_cursor->size().width(),
                 mouse_cursor->size().height(),
                 mouse_cursor->stride(),
                 QImage::Format::Format_ARGB32);

    desktop_->setCursor(QCursor(QPixmap::fromImage(image),
                                mouse_cursor->hotSpot().x(),
                                mouse_cursor->hotSpot().y()));
}

void QtDesktopWindow::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    clipboard_->injectClipboardEvent(event);
}

void QtDesktopWindow::onPointerEvent(const QPoint& pos, uint32_t mask)
{
    if (panel_->autoScrolling() && panel_->scale() != -1)
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
            if (cursor.x() > client_area.width() - 150)
                scroll_delta_.setX(10);
            else if (cursor.x() < 150)
                scroll_delta_.setX(-10);
        }

        if (client_area.height() < desktop_->height())
        {
            if (cursor.y() > client_area.height() - 150)
                scroll_delta_.setY(10);
            else if (cursor.y() < 150)
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

    desktop::Frame* current_frame = desktop_->desktopFrame();
    if (current_frame)
    {
        const desktop::Size& source_size = current_frame->size();
        QSize scaled_size = desktop_->size();

        double scale_x = (scaled_size.width() * 100) / static_cast<double>(source_size.width());
        double scale_y = (scaled_size.height() * 100) / static_cast<double>(source_size.height());
        double scale = std::min(scale_x, scale_y);

        proto::PointerEvent pointer_event;

        pointer_event.set_mask(mask);
        pointer_event.set_x((static_cast<double>(pos.x() * 100) / scale) + screen_top_left_.x());
        pointer_event.set_y((static_cast<double>(pos.y() * 100) / scale) + screen_top_left_.y());

        desktop_control_proxy_->onPointerEvent(pointer_event);
    }
}

void QtDesktopWindow::onKeyEvent(uint32_t usb_keycode, uint32_t flags)
{
    proto::KeyEvent key_event;

    key_event.set_usb_keycode(usb_keycode);
    key_event.set_flags(flags);

    desktop_control_proxy_->onKeyEvent(key_event);
}

void QtDesktopWindow::onDrawDesktop()
{
    panel_->update();
}

void QtDesktopWindow::changeSettings()
{
    DesktopConfigDialog* dialog = new DesktopConfigDialog(
        session_type_, desktop_config_, video_encodings_, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &DesktopConfigDialog::configChanged, this, &QtDesktopWindow::onConfigChanged);

    dialog->show();
    dialog->activateWindow();
}

void QtDesktopWindow::onConfigChanged(const proto::DesktopConfig& desktop_config)
{
    desktop_config_ = desktop_config;
    desktop_control_proxy_->setDesktopConfig(desktop_config);
}

void QtDesktopWindow::autosizeWindow()
{
    desktop::Frame* frame = desktop_->desktopFrame();
    if (!frame)
        return;

    QSize frame_size(desktop_->desktopFrame()->size().width(),
                     desktop_->desktopFrame()->size().height());
    QRect local_screen_rect = QApplication::desktop()->availableGeometry(this);
    QSize window_size = frame_size + frameSize() - size();

    if (window_size.width() < local_screen_rect.width() &&
        window_size.height() < local_screen_rect.height())
    {
        QSize remote_screen_size = scaledSize(frame_size, panel_->scale());

        showNormal();

        resize(remote_screen_size);
        move(local_screen_rect.x() + (local_screen_rect.width() / 2 - window_size.width() / 2),
             local_screen_rect.y() + (local_screen_rect.height() / 2 - window_size.height() / 2));
    }
    else
    {
        showMaximized();
    }
}

void QtDesktopWindow::takeScreenshot()
{
    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(this,
                                                     tr("Save File"),
                                                     QString(),
                                                     tr("PNG Image (*.png);;BMP Image (*.bmp)"),
                                                     &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
        return;

    FrameQImage* frame = static_cast<FrameQImage*>(desktop_->desktopFrame());
    if (!frame)
        return;

    const char* format = nullptr;

    if (selected_filter.contains(QLatin1String("*.png")))
        format = "PNG";
    else if (selected_filter.contains(QLatin1String("*.bmp")))
        format = "BMP";

    if (!format)
        return;

    if (!frame->constImage().save(file_path, format))
        QMessageBox::warning(this, tr("Warning"), tr("Could not save image"), QMessageBox::Ok);
}

void QtDesktopWindow::scaleDesktop()
{
    desktop::Frame* current_frame = desktop_->desktopFrame();
    if (!current_frame)
        return;

    QSize source_size(current_frame->size().width(), current_frame->size().height());
    QSize target_size;

    int scale = panel_->scale();
    if (scale != -1)
        target_size = scaledSize(source_size, scale);
    else
        target_size = size();

    desktop_->resize(source_size.scaled(target_size, Qt::KeepAspectRatio));
}

void QtDesktopWindow::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == scroll_timer_id_)
    {
        if (scroll_delta_.x() != 0)
        {
            QScrollBar* scrollbar = scroll_area_->horizontalScrollBar();

            int pos = scrollbar->sliderPosition() + scroll_delta_.x();

            pos = std::max(pos, scrollbar->minimum());
            pos = std::min(pos, scrollbar->maximum());

            scrollbar->setSliderPosition(pos);
        }

        if (scroll_delta_.y() != 0)
        {
            QScrollBar* scrollbar = scroll_area_->verticalScrollBar();

            int pos = scrollbar->sliderPosition() + scroll_delta_.y();

            pos = std::max(pos, scrollbar->minimum());
            pos = std::min(pos, scrollbar->maximum());

            scrollbar->setSliderPosition(pos);
        }
    }

    QWidget::timerEvent(event);
}

void QtDesktopWindow::resizeEvent(QResizeEvent* event)
{
    panel_->move(QPoint(width() / 2 - panel_->width() / 2, 0));
    scaleDesktop();

    QWidget::resizeEvent(event);
}

void QtDesktopWindow::leaveEvent(QEvent* event)
{
    if (scroll_timer_id_)
    {
        killTimer(scroll_timer_id_);
        scroll_timer_id_ = 0;
    }

    QWidget::leaveEvent(event);
}

bool QtDesktopWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == desktop_)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_Tab)
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
            QWheelEvent* wheel_event = static_cast<QWheelEvent*>(event);
            QPoint pos = desktop_->mapFromGlobal(wheel_event->globalPos());

            desktop_->doMouseEvent(wheel_event->type(),
                                   wheel_event->buttons(),
                                   pos,
                                   wheel_event->angleDelta());
            return true;
        }
    }

    return QWidget::eventFilter(object, event);
}

void QtDesktopWindow::onClipboardEvent(const proto::ClipboardEvent& event)
{
    desktop_control_proxy_->onClipboardEvent(event);
}

} // namespace client
