//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/desktop_window.h"

#include <QDebug>
#include <QApplication>
#include <QBrush>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>

#include "base/clipboard.h"
#include "client/ui/desktop_config_dialog.h"
#include "client/ui/desktop_panel.h"
#include "client/ui/desktop_widget.h"
#include "desktop_capture/desktop_frame_qimage.h"

namespace aspia {

DesktopWindow::DesktopWindow(proto::Computer* computer, QWidget* parent)
    : QWidget(parent),
      computer_(computer)
{
    QString session_name;
    if (computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
    {
        session_name = tr("Aspia Desktop Manage");
    }
    else
    {
        Q_ASSERT(computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_VIEW);
        session_name = tr("Aspia Desktop View");
    }

    QString computer_name;
    if (!computer_->name().empty())
        computer_name = QString::fromStdString(computer_->name());
    else
        computer_name = QString::fromStdString(computer_->address());

    setWindowTitle(QString("%1 - %2").arg(computer_name).arg(session_name));
    setWindowIcon(QIcon(QStringLiteral(":/icon/main.png")));
    setMinimumSize(800, 600);

    desktop_ = new DesktopWidget(this);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    scroll_area_->setFrameShape(QFrame::NoFrame);
    scroll_area_->setAutoFillBackground(true);
    scroll_area_->setWidget(desktop_);

    QPalette palette(scroll_area_->palette());
    palette.setBrush(QPalette::Background, QBrush(QColor(25, 25, 25)));
    scroll_area_->setPalette(palette);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->addWidget(scroll_area_);

    panel_ = new DesktopPanel(computer_->session_type(), this);
    panel_->adjustSize();

    connect(panel_, &DesktopPanel::keySequence, desktop_, &DesktopWidget::executeKeySequense);
    connect(panel_, &DesktopPanel::settingsButton, this, &DesktopWindow::changeSettings);
    connect(panel_, &DesktopPanel::switchToAutosize, this, &DesktopWindow::autosizeWindow);

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

    connect(desktop_, &DesktopWidget::sendPointerEvent, this, &DesktopWindow::onPointerEvent);
    connect(desktop_, &DesktopWidget::sendKeyEvent, this, &DesktopWindow::sendKeyEvent);
    connect(desktop_, &DesktopWidget::updated, panel_, QOverload<>::of(&DesktopPanel::update));

    if (computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE &&
        (computer_->desktop_manage_session().flags() & proto::desktop::Config::ENABLE_CLIPBOARD))
    {
        clipboard_ = new Clipboard(this);
        connect(clipboard_, &Clipboard::clipboardEvent, this, &DesktopWindow::sendClipboardEvent);
    }

    desktop_->installEventFilter(this);
}

void DesktopWindow::resizeDesktopFrame(const QSize& screen_size)
{
    QSize prev_size = desktop_->size();

    desktop_->resizeDesktopFrame(screen_size);

    if (screen_size != prev_size && !isMaximized() && !isFullScreen())
        autosizeWindow();
}

void DesktopWindow::drawDesktopFrame()
{
    desktop_->drawDesktopFrame();
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

void DesktopWindow::onPointerEvent(const QPoint& pos, quint32 mask)
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

    emit sendPointerEvent(pos, mask);
}

void DesktopWindow::changeSettings()
{
    proto::desktop::Config* config = config = computer_->mutable_desktop_view_session();

    if (computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        config = computer_->mutable_desktop_manage_session();

    DesktopConfigDialog dialog(computer_->session_type(), config, this);
    if (dialog.exec() == DesktopConfigDialog::Accepted)
    {
        if (computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        {
            delete clipboard_;

            if (config->flags() & proto::desktop::Config::ENABLE_CLIPBOARD)
            {
                clipboard_ = new Clipboard(this);

                connect(clipboard_, &Clipboard::clipboardEvent,
                        this, &DesktopWindow::sendClipboardEvent);
            }
        }

        emit sendConfig(*config);
    }
}

void DesktopWindow::autosizeWindow()
{
    QRect desktop_rect = QApplication::desktop()->screenGeometry();
    QRect full_rect = geometry();
    QRect client_rect = rect();

    QSize window_size(desktop_->size().width() + full_rect.width() - client_rect.width(),
                      desktop_->size().height() + full_rect.height() - client_rect.height());

    if (window_size.width() < desktop_rect.width() && window_size.height() < desktop_rect.height())
    {
        showNormal();

        resize(window_size.width(), window_size.height());
        move(desktop_rect.width() / 2 - window_size.width() / 2,
             desktop_rect.height() / 2 - window_size.height() / 2);
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
    emit windowClose();
    QWidget::closeEvent(event);
}

bool DesktopWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == desktop_)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent* key_event = dynamic_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_Tab)
            {
                desktop_->doKeyEvent(key_event);
                return true;
            }
        }

        return false;
    }

    return QWidget::eventFilter(object, event);
}

} // namespace aspia
