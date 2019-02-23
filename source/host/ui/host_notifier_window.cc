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

#include "host/ui/host_notifier_window.h"

#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTranslator>

#include "base/logging.h"
#include "build/build_config.h"

namespace host {

namespace {

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const proto::notifier::ConnectEvent& event)
        : event_(event)
    {
        switch (event_.session_type())
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
                setIcon(0, QIcon(QStringLiteral(":/img/monitor-keyboard.png")));
                break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
                setIcon(0, QIcon(QStringLiteral(":/img/monitor.png")));
                break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                setIcon(0, QIcon(QStringLiteral(":/img/folder-stand.png")));
                break;

            default:
                LOG(LS_FATAL) << "Unexpected session type: " << event_.session_type();
                return;
        }

        setText(0, QString("%1 (%2)")
                .arg(QString::fromStdString(event_.username()))
                .arg(QString::fromStdString(event_.remote_address())));
    }

    const proto::notifier::ConnectEvent& event() const { return event_; }

private:
    proto::notifier::ConnectEvent event_;
    DISALLOW_COPY_AND_ASSIGN(SessionTreeItem);
};

} // namespace

HostNotifierWindow::HostNotifierWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    ui.setupUi(this);

    ui.label_title->installEventFilter(this);
    ui.label_connections->installEventFilter(this);

    connect(ui.button_show_hide, &QPushButton::released,
            this, &HostNotifierWindow::onShowHidePressed);

    connect(ui.button_disconnect_all, &QPushButton::released,
            this, &HostNotifierWindow::onDisconnectAllPressed);

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &HostNotifierWindow::onContextMenu);

    setAttribute(Qt::WA_TranslucentBackground);

    connect(QApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, &HostNotifierWindow::updateWindowPosition);

    updateWindowPosition();
}

void HostNotifierWindow::onConnectEvent(const proto::notifier::ConnectEvent& event)
{
    ui.tree->addTopLevelItem(new SessionTreeItem(event));
    ui.button_disconnect_all->setEnabled(true);
}

void HostNotifierWindow::onDisconnectEvent(const proto::notifier::DisconnectEvent& event)
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = dynamic_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item && item->event().uuid() == event.uuid())
        {
            delete item;
            break;
        }
    }

    if (!ui.tree->topLevelItemCount())
        close();
}

bool HostNotifierWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui.label_title || object == ui.label_connections)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
                if (mouse_event && mouse_event->button() == Qt::LeftButton)
                {
                    start_pos_ = mouse_event->pos();
                    return true;
                }
            }
            break;

            case QEvent::MouseMove:
            {
                QMouseEvent* mouse_event = dynamic_cast<QMouseEvent*>(event);
                if (mouse_event && mouse_event->buttons() & Qt::LeftButton && start_pos_.x() >= 0)
                {
                    QPoint diff = mouse_event->pos() - start_pos_;
                    move(pos() + diff);
                    return true;
                }
            }
            break;

            case QEvent::MouseButtonRelease:
            {
                start_pos_ = QPoint(-1, -1);
            }
            break;

            default:
                break;
        }
    }

    return QWidget::eventFilter(object, event);
}

void HostNotifierWindow::hideEvent(QHideEvent* event)
{
    show();
}

void HostNotifierWindow::onShowHidePressed()
{
    if (ui.content->isVisible())
        hideNotifier();
    else
        showNotifier();
}

void HostNotifierWindow::onDisconnectAllPressed()
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = dynamic_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item)
            emit killSession(item->event().uuid());
    }
}

void HostNotifierWindow::onContextMenu(const QPoint& point)
{
    SessionTreeItem* item = dynamic_cast<SessionTreeItem*>(ui.tree->itemAt(point));
    if (!item)
        return;

    QAction disconnect_action(tr("Disconnect"));

    QMenu menu;
    menu.addAction(&disconnect_action);

    if (menu.exec(ui.tree->viewport()->mapToGlobal(point)) == &disconnect_action)
        emit killSession(item->event().uuid());
}

void HostNotifierWindow::updateWindowPosition()
{
    showNotifier();

    QRect screen_rect = QApplication::primaryScreen()->availableGeometry();
    QSize window_size = frameSize();

    move(screen_rect.x() + (screen_rect.width() - window_size.width()) - 1,
         screen_rect.y() + (screen_rect.height() - window_size.height()) - 1);
}

void HostNotifierWindow::showNotifier()
{
    if (ui.content->isHidden() && ui.title->isHidden())
    {
        ui.content->show();
        ui.title->show();

        move(window_rect_.topLeft());
        setFixedSize(window_rect_.size());

        ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/img/arrow-left-gray.png")));
    }
}

void HostNotifierWindow::hideNotifier()
{
    QRect screen_rect = QApplication::primaryScreen()->availableGeometry();
    QSize content_size = ui.content->frameSize();
    window_rect_ = frameGeometry();

    ui.content->hide();
    ui.title->hide();

    move(screen_rect.x() + screen_rect.width() - ui.button_show_hide->width(), pos().y());

    setFixedSize(window_rect_.width() - content_size.width(),
                 window_rect_.height());

    ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/img/arrow-right-gray.png")));
}

} // namespace host
