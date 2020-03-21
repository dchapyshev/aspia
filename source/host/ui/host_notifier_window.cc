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

#include "host/ui/host_notifier_window.h"

#include "base/logging.h"
#include "build/build_config.h"

#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTranslator>

namespace host {

namespace {

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const UserSessionAgent::Client& client)
        : uuid_(client.uuid)
    {
        switch (client.session_type)
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
                LOG(LS_FATAL) << "Unexpected session type: " << client.session_type;
                return;
        }

        setText(0, QString("%1 (%2)")
                .arg(QString::fromStdString(client.username))
                .arg(QString::fromStdString(client.address)));
    }

    const std::string& uuid() const { return uuid_; }

private:
    const std::string uuid_;
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

    updateWindowPosition();
}

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
            emit killSession(item->uuid());
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

void NotifierWindow::hideEvent(QHideEvent* event)
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
        emit killSession(item->uuid());
}

void NotifierWindow::updateWindowPosition()
{
    showNotifier();

    QRect screen_rect = QApplication::primaryScreen()->availableGeometry();
    QSize window_size = frameSize();

    move(screen_rect.x() + (screen_rect.width() - window_size.width()) - 1,
         screen_rect.y() + (screen_rect.height() - window_size.height()) - 1);
}

void NotifierWindow::showNotifier()
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

void NotifierWindow::hideNotifier()
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
