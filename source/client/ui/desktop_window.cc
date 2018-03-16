//
// PROJECT:         Aspia
// FILE:            client/ui/desktop_window.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/desktop_window.h"

#include <QDebug>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QDesktopWidget>
#include <QMimeData>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShortcut>
#include <QTimer>

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
    {
        computer_name = QString::fromUtf8(computer_->name().c_str(),
                                          computer_->name().size());
    }
    else
    {
        computer_name = QString::fromUtf8(computer_->address().c_str(),
                                          computer_->address().size());
    }

    setWindowTitle(QString("%1 - %2").arg(computer_name).arg(session_name));
    setWindowIcon(QIcon(":/icon/main.png"));
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

    panel_ = new DesktopPanel(this);

    scroll_timer_ = new QTimer(this);

    connect(scroll_timer_, SIGNAL(timeout()), SLOT(onScrollTimeout()));

    connect(QApplication::clipboard(), SIGNAL(dataChanged()), SLOT(clipboardDataChanged()));

    connect(panel_, SIGNAL(keySequence(int)), desktop_, SLOT(executeKeySequense(int)));
    connect(panel_, SIGNAL(switchToFullscreen()), SLOT(showFullScreen()));
    connect(panel_, SIGNAL(switchToNormal()), SLOT(showNormal()));
    connect(panel_, SIGNAL(switchToAutosize()), SLOT(autosizeWindow()));
    connect(panel_, SIGNAL(settingsButton()), SLOT(changeSettings()));
    connect(panel_, SIGNAL(closeButton()), SLOT(close()));

    connect(desktop_, SIGNAL(sendPointerEvent(const QPoint&, quint32)),
            SLOT(onPointerEvent(const QPoint&, quint32)));

    connect(desktop_, SIGNAL(sendKeyEvent(quint32, quint32)),
            SIGNAL(sendKeyEvent(quint32, quint32)));
}

void DesktopWindow::resizeDesktopFrame(const QSize& screen_size)
{
    QSize prev_size = desktop_->size();

    desktop_->resizeDesktopFrame(screen_size);

    if (screen_size != prev_size)
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

void DesktopWindow::injectClipboard(const QString& text)
{
    QApplication::clipboard()->setText(text);
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

    scroll_delta_ = QPoint();

    if (client_area.width() < desktop_->width())
    {
        if (cursor.x() > client_area.width() - 50)
            scroll_delta_.setX(20);
        else if (cursor.x() < 50)
            scroll_delta_.setX(-20);
    }

    if (client_area.height() < desktop_->height())
    {
        if (cursor.y() > client_area.height() - 50)
            scroll_delta_.setY(20);
        else if (cursor.y() < 50)
            scroll_delta_.setY(-20);
    }

    if (!scroll_delta_.isNull())
        scroll_timer_->start(20);
    else
        scroll_timer_->stop();

    emit sendPointerEvent(pos, mask);
}

void DesktopWindow::onScrollTimeout()
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

void DesktopWindow::clipboardDataChanged()
{
    const QMimeData* mime_data = QApplication::clipboard()->mimeData();
    if (!mime_data->hasText())
        return;

    emit sendClipboardEvent(mime_data->text());
}

void DesktopWindow::changeSettings()
{
    proto::desktop::Config* config = config = computer_->mutable_desktop_view_session();

    if (computer_->session_type() == proto::auth::SESSION_TYPE_DESKTOP_MANAGE)
        config = computer_->mutable_desktop_manage_session();

    DesktopConfigDialog dialog(computer_->session_type(), config, this);
    if (dialog.exec() == DesktopConfigDialog::Accepted)
        emit sendConfig(*config);
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
        if (!isMaximized())
        {
            move(desktop_rect.width() / 2 - window_size.width() / 2,
                 desktop_rect.height() / 2 - window_size.height() / 2);
            resize(window_size.width(), window_size.height());
        }

        return;
    }

    if (!isMaximized())
        showMaximized();
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

} // namespace aspia
