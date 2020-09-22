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

#include "host/ui/notifier_window.h"

#include "build/build_config.h"
#include "qt_base/qt_logging.h"

#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QTranslator>

namespace host {

namespace {

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const UserSessionAgent::Client& client)
        : id_(client.id)
    {
        switch (client.session_type)
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
                setIcon(0, QIcon(":/img/monitor-keyboard.png"));
                break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
                setIcon(0, QIcon(":/img/monitor.png"));
                break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                setIcon(0, QIcon(":/img/folder-stand.png"));
                break;

            default:
                LOG(LS_FATAL) << "Unexpected session type: " << client.session_type;
                return;
        }

        setText(0, QString("%1 (%2)")
                .arg(QString::fromStdString(client.username))
                .arg(QString::fromStdString(client.computer_name)));
    }

    uint32_t id() const { return id_; }

private:
    const uint32_t id_;
    DISALLOW_COPY_AND_ASSIGN(SessionTreeItem);
};

} // namespace

NotifierWindow::NotifierWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    ui.setupUi(this);

    ui.label_title->installEventFilter(this);
    ui.label_connections->installEventFilter(this);

    connect(ui.button_show_hide, &QPushButton::released,
            this, &NotifierWindow::onShowHidePressed);

    connect(ui.button_disconnect_all, &QPushButton::released,
            this, &NotifierWindow::disconnectAll);

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &NotifierWindow::onContextMenu);

    setAttribute(Qt::WA_TranslucentBackground);

    connect(QApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, &NotifierWindow::updateWindowPosition);

    QTimer::singleShot(std::chrono::milliseconds(15), this, &NotifierWindow::updateWindowPosition);
}

NotifierWindow::~NotifierWindow() = default;

void NotifierWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!clients.empty())
    {
        ui.tree->clear();

        for (const auto& client : clients)
            ui.tree->addTopLevelItem(new SessionTreeItem(client));
    }
    else
    {
        LOG(LS_INFO) << "Empty session list. Notifier closed";

        emit finished();
        close();
    }
}

void NotifierWindow::disconnectAll()
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = static_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item)
        {
            LOG(LS_INFO) << "Disconnect session with ID: " << item->id();
            emit killSession(item->id());
        }
    }
}

bool NotifierWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui.label_title || object == ui.label_connections)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::LeftButton)
                {
                    start_pos_ = mouse_event->pos();
                    return true;
                }
            }
            break;

            case QEvent::MouseMove:
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->buttons() & Qt::LeftButton && start_pos_.x() >= 0)
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

void NotifierWindow::hideEvent(QHideEvent* /* event */)
{
    show();
}

void NotifierWindow::onShowHidePressed()
{
    if (ui.content->isVisible())
        hideNotifier();
    else
        showNotifier();
}

void NotifierWindow::onContextMenu(const QPoint& point)
{
    SessionTreeItem* item = static_cast<SessionTreeItem*>(ui.tree->itemAt(point));
    if (!item)
        return;

    QAction disconnect_action(tr("Disconnect"));

    QMenu menu;
    menu.addAction(&disconnect_action);

    if (menu.exec(ui.tree->viewport()->mapToGlobal(point)) == &disconnect_action)
    {
        LOG(LS_INFO) << "Disconnect session with ID: " << item->id();
        emit killSession(item->id());
    }
}

void NotifierWindow::updateWindowPosition()
{
    showNotifier();

    QScreen* primary_screen = QApplication::primaryScreen();
    if (!primary_screen)
    {
        LOG(LS_ERROR) << "Primary screen not available";
        return;
    }

    QRect available_rect = primary_screen->availableGeometry();
    QSize window_size = frameSize();

    int x = available_rect.x() + (available_rect.width() - window_size.width());
    int y = available_rect.y() + (available_rect.height() - window_size.height());

    LOG(LS_INFO) << "Available geometry for primary screen has been changed: " << available_rect;
    LOG(LS_INFO) << "Full primary screen geometry: " << primary_screen->geometry();
    LOG(LS_INFO) << "Notifier window size: " << window_size;
    LOG(LS_INFO) << "Notifier window moved to: " << x << "x" << y;

    move(x, y);
}

void NotifierWindow::showNotifier()
{
    LOG(LS_INFO) << "showNotifier called";

    if (ui.content->isHidden() && ui.title->isHidden())
    {
        ui.content->show();
        ui.title->show();

        QPoint window_pos = window_rect_.topLeft();
        QSize window_size = window_rect_.size();

        LOG(LS_INFO) << "Notifier window size: " << window_size;
        LOG(LS_INFO) << "Notifier window moved to: " << window_pos;

        move(window_pos);
        setFixedSize(window_size);

        ui.button_show_hide->setIcon(QIcon(":/img/arrow-left-gray.png"));
    }
    else
    {
        LOG(LS_INFO) << "Notifier already visibled";
    }
}

void NotifierWindow::hideNotifier()
{
    LOG(LS_INFO) << "hideNotifier called";

    QScreen* primary_screen = QApplication::primaryScreen();
    if (!primary_screen)
    {
        LOG(LS_ERROR) << "Primary screen not available";
        return;
    }

    QRect screen_rect = primary_screen->availableGeometry();
    QSize content_size = ui.content->frameSize();
    window_rect_ = frameGeometry();

    ui.content->hide();
    ui.title->hide();

    QPoint window_pos(screen_rect.x() + screen_rect.width() - ui.button_show_hide->width(), pos().y());
    QSize window_size(window_rect_.width() - content_size.width(), window_rect_.height());

    LOG(LS_INFO) << "Notifier window size: " << window_size;
    LOG(LS_INFO) << "Notifier window moved to: " << window_pos;

    move(window_pos);
    setFixedSize(window_size);

    ui.button_show_hide->setIcon(QIcon(":/img/arrow-right-gray.png"));
}

} // namespace host
