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
#include "base/desktop/mouse_cursor.h"
#include "base/strings/string_split.h"
#include "client/client_desktop.h"
#include "client/desktop_control_proxy.h"
#include "client/desktop_window_proxy.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/ui/desktop_panel.h"
#include "client/ui/frame_factory_qimage.h"
#include "client/ui/frame_qimage.h"
#include "client/ui/qt_file_manager_window.h"
#include "client/ui/system_info_window.h"
#include "client/ui/statistics_dialog.h"
#include "common/desktop_session_constants.h"
#include "qt_base/application.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QWindow>

namespace client {

namespace {

QSize scaledSize(const QSize& source_size, int scale)
{
    if (scale == -1)
        return source_size;

    int width = static_cast<int>(static_cast<double>(source_size.width() * scale) / 100.0);
    int height = static_cast<int>(static_cast<double>(source_size.height() * scale) / 100.0);

    return QSize(width, height);
}

} // namespace

QtDesktopWindow::QtDesktopWindow(proto::SessionType session_type,
                                 const proto::DesktopConfig& desktop_config,
                                 QWidget* parent)
    : SessionWindow(parent),
      session_type_(session_type),
      desktop_config_(desktop_config),
      desktop_window_proxy_(std::make_shared<DesktopWindowProxy>(
          qt_base::Application::uiTaskRunner(), this))
{
    setMinimumSize(400, 300);

    desktop_ = new DesktopWidget(this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setWidget(desktop_);

    QPalette palette(scroll_area_->palette());
    palette.setBrush(QPalette::Window, QBrush(QColor(25, 25, 25)));
    scroll_area_->viewport()->setPalette(palette);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    panel_ = new DesktopPanel(session_type_, scroll_area_);
    panel_->installEventFilter(this);

    resize_timer_ = new QTimer(this);
    connect(resize_timer_, &QTimer::timeout, this, &QtDesktopWindow::onResizeTimer);

    scroll_timer_ = new QTimer(this);
    connect(scroll_timer_, &QTimer::timeout, this, &QtDesktopWindow::onScrollTimer);

    desktop_->enableKeyCombinations(panel_->sendKeyCombinations());

    connect(panel_, &DesktopPanel::keyCombination, desktop_, &DesktopWidget::executeKeyCombination);
    connect(panel_, &DesktopPanel::settingsButton, this, &QtDesktopWindow::changeSettings);
    connect(panel_, &DesktopPanel::switchToAutosize, this, &QtDesktopWindow::autosizeWindow);
    connect(panel_, &DesktopPanel::takeScreenshot, this, &QtDesktopWindow::takeScreenshot);
    connect(panel_, &DesktopPanel::scaleChanged, this, &QtDesktopWindow::scaleDesktop);
    connect(panel_, &DesktopPanel::minimizeSession, this, &QtDesktopWindow::showMinimized);
    connect(panel_, &DesktopPanel::closeSession, this, &QtDesktopWindow::close);

    connect(panel_, &DesktopPanel::screenSelected, this, [this](const proto::Screen& screen)
    {
        desktop_control_proxy_->setCurrentScreen(screen);
    });

    connect(panel_, &DesktopPanel::powerControl, this, [this](proto::PowerControl::Action action)
    {
        desktop_control_proxy_->onPowerControl(action);
    });

    connect(panel_, &DesktopPanel::startRemoteUpdate, this, [this]()
    {
        desktop_control_proxy_->onRemoteUpdate();
    });

    connect(panel_, &DesktopPanel::startSystemInfo, this, [this]()
    {
        desktop_control_proxy_->onSystemInfoRequest();
    });

    connect(panel_, &DesktopPanel::startStatistics, this, [this]()
    {
        desktop_control_proxy_->onMetricsRequest();
    });

    connect(panel_, &DesktopPanel::pasteAsKeystrokes, this, &QtDesktopWindow::onPasteKeystrokes);
    connect(panel_, &DesktopPanel::switchToFullscreen, this, [this](bool fullscreen)
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

    connect(panel_, &DesktopPanel::startSession, this, [this](proto::SessionType session_type)
    {
        client::Config session_config = config();
        session_config.session_type = session_type;

        client::SessionWindow* session_window = nullptr;

        switch (session_config.session_type)
        {
            case proto::SESSION_TYPE_FILE_TRANSFER:
                session_window = new client::QtFileManagerWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!session_window)
            return;

        session_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!session_window->connectToHost(session_config))
            session_window->close();
    });

    connect(desktop_, &DesktopWidget::sig_mouseEvent, this, &QtDesktopWindow::onMouseEvent);
    connect(desktop_, &DesktopWidget::sig_keyEvent, this, &QtDesktopWindow::onKeyEvent);

    desktop_->setFocus();
}

QtDesktopWindow::~QtDesktopWindow()
{
    desktop_window_proxy_->dettach();
}

std::unique_ptr<Client> QtDesktopWindow::createClient()
{
    std::unique_ptr<ClientDesktop> client = std::make_unique<ClientDesktop>(
        qt_base::Application::ioTaskRunner());

    client->setDesktopConfig(desktop_config_);
    client->setDesktopWindow(desktop_window_proxy_);

    return std::move(client);
}

void QtDesktopWindow::showWindow(
    std::shared_ptr<DesktopControlProxy> desktop_control_proxy, const base::Version& peer_version)
{
    desktop_control_proxy_ = std::move(desktop_control_proxy);
    peer_version_ = peer_version;

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

        connect(system_info_, &SystemInfoWindow::systemInfoRequired, this, [this]()
        {
            desktop_control_proxy_->onSystemInfoRequest();
        });
    }

    system_info_->setSystemInfo(system_info);
    system_info_->show();
    system_info_->activateWindow();
}

void QtDesktopWindow::setMetrics(const DesktopWindow::Metrics& metrics)
{
    if (!statistics_dialog_)
    {
        statistics_dialog_ = new StatisticsDialog(this);
        statistics_dialog_->setAttribute(Qt::WA_DeleteOnClose);

        connect(statistics_dialog_, &StatisticsDialog::metricsRequired, this, [this]()
        {
            desktop_control_proxy_->onMetricsRequest();
        });

        statistics_dialog_->show();
        statistics_dialog_->activateWindow();
    }

    statistics_dialog_->setMetrics(metrics);
}

std::unique_ptr<FrameFactory> QtDesktopWindow::frameFactory()
{
    return std::make_unique<FrameFactoryQImage>();
}

void QtDesktopWindow::setFrame(
    const base::Size& screen_size, std::shared_ptr<base::Frame> frame)
{
    screen_size_ = QSize(screen_size.width(), screen_size.height());

    bool resize = desktop_->desktopFrame() == nullptr;

    desktop_->setDesktopFrame(frame);
    scaleDesktop();

    if (resize)
    {
        LOG(LS_INFO) << "Resize window (first frame)";
        autosizeWindow();
    }
}

void QtDesktopWindow::drawFrame()
{
    desktop_->update();
    panel_->update();
}

void QtDesktopWindow::setMouseCursor(std::shared_ptr<base::MouseCursor> mouse_cursor)
{
    QImage image(mouse_cursor->constImage().data(),
                 mouse_cursor->width(),
                 mouse_cursor->height(),
                 mouse_cursor->stride(),
                 QImage::Format::Format_ARGB32);

    desktop_->setCursor(QCursor(
        QPixmap::fromImage(std::move(image)), mouse_cursor->hotSpotX(), mouse_cursor->hotSpotY()));
}

void QtDesktopWindow::resizeEvent(QResizeEvent* event)
{
    panel_->move(QPoint(width() / 2 - panel_->width() / 2, 0));
    scaleDesktop();
    QWidget::resizeEvent(event);
}

void QtDesktopWindow::leaveEvent(QEvent* event)
{
    if (scroll_timer_->isActive())
        scroll_timer_->stop();

    QWidget::leaveEvent(event);
}

void QtDesktopWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && isMinimized())
        desktop_->userLeftFromWindow();
    QWidget::changeEvent(event);
}

void QtDesktopWindow::focusOutEvent(QFocusEvent* event)
{
    desktop_->userLeftFromWindow();
    QWidget::focusOutEvent(event);
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
            QPoint pos = desktop_->mapFromGlobal(wheel_event->globalPosition().toPoint());

            desktop_->doMouseEvent(wheel_event->type(),
                                   wheel_event->buttons(),
                                   pos,
                                   wheel_event->angleDelta());
            return true;
        }
    }
    else if (object == panel_)
    {
        if (event->type() == QEvent::Resize)
        {
            panel_->move(QPoint(width() / 2 - panel_->width() / 2, 0));
        }
        else if (event->type() == QEvent::Leave)
        {
            desktop_->setFocus();
        }
    }

    return QWidget::eventFilter(object, event);
}

void QtDesktopWindow::onMouseEvent(const proto::MouseEvent& event)
{
    QPoint pos(event.x(), event.y());

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

        static const int kThresholdDistance = 80;
        static const int kStep = 10;

        if (client_area.width() < desktop_->width())
        {
            if (cursor.x() > client_area.width() - kThresholdDistance)
                scroll_delta_.setX(kStep);
            else if (cursor.x() < kThresholdDistance)
                scroll_delta_.setX(-kStep);
        }

        if (client_area.height() < desktop_->height())
        {
            if (cursor.y() > client_area.height() - kThresholdDistance)
                scroll_delta_.setY(kStep);
            else if (cursor.y() < kThresholdDistance)
                scroll_delta_.setY(-kStep);
        }

        if (!scroll_delta_.isNull())
        {
            if (!scroll_timer_->isActive())
                scroll_timer_->start(std::chrono::milliseconds(15));
        }
        else if (scroll_timer_->isActive())
        {
            scroll_timer_->stop();
        }
    }
    else if (scroll_timer_->isActive())
    {
        scroll_timer_->stop();
    }

    base::Frame* current_frame = desktop_->desktopFrame();
    if (current_frame)
    {
        const base::Size& source_size = current_frame->size();
        QSize scaled_size = desktop_->size();

        double scale_x = (scaled_size.width() * 100) / static_cast<double>(source_size.width());
        double scale_y = (scaled_size.height() * 100) / static_cast<double>(source_size.height());
        double scale = std::min(scale_x, scale_y);

        proto::MouseEvent out_event;

        out_event.set_mask(event.mask());
        out_event.set_x(static_cast<int>(static_cast<double>(pos.x() * 100) / scale));
        out_event.set_y(static_cast<int>(static_cast<double>(pos.y() * 100) / scale));

        desktop_control_proxy_->onMouseEvent(out_event);
    }

    // In MacOS event Leave does not always come to the widget when the mouse leaves its area.
    if (!panel_->isPanelHidden() && !panel_->isPanelPinned())
    {
        if (!panel_->rect().contains(pos))
            QApplication::postEvent(panel_, new QEvent(QEvent::Leave));
    }
}

void QtDesktopWindow::onKeyEvent(const proto::KeyEvent& event)
{
    desktop_control_proxy_->onKeyEvent(event);
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
    if (screen_size_.isEmpty())
        return;

    QRect local_screen_rect = QApplication::desktop()->availableGeometry(this);
    QSize window_size = screen_size_ + frameSize() - size();

    if (window_size.width() < local_screen_rect.width() &&
        window_size.height() < local_screen_rect.height())
    {
        QSize remote_screen_size = scaledSize(screen_size_, panel_->scale());

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
    if (screen_size_.isEmpty())
    {
        LOG(LS_INFO) << "No screen size";
        return;
    }

    QSize source_size(screen_size_);
    QSize target_size(size());

    int scale = panel_->scale();
    if (scale != -1)
        target_size = scaledSize(source_size, scale);

    desktop_->resize(source_size.scaled(target_size, Qt::KeepAspectRatio));

    if (resize_timer_->isActive())
        resize_timer_->stop();

    LOG(LS_INFO) << "Starting resize timer";
    resize_timer_->start(std::chrono::milliseconds(500));
}

void QtDesktopWindow::onResizeTimer()
{
    QSize desktop_size = desktop_->size();

    QScreen* current_screen = window()->windowHandle()->screen();
    if (current_screen)
        desktop_size *= current_screen->devicePixelRatio();

    LOG(LS_INFO) << "Resize timer: " << desktop_size.width() << "x" << desktop_size.height();

    desktop_control_proxy_->setPreferredSize(desktop_size.width(), desktop_size.height());
    resize_timer_->stop();
}

void QtDesktopWindow::onScrollTimer()
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

void QtDesktopWindow::onPasteKeystrokes()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard)
    {
        QString text = clipboard->text();
        if (text.isEmpty())
        {
            LOG(LS_INFO) << "Empty clipboard";
            return;
        }

        proto::TextEvent event;
        event.set_text(text.toStdString());

        desktop_control_proxy_->onTextEvent(event);
    }
    else
    {
        LOG(LS_WARNING) << "QApplication::clipboard failed";
    }
}

} // namespace client
