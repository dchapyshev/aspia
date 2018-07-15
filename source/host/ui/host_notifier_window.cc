//
// PROJECT:         Aspia
// FILE:            host/host_notifier_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ui/host_notifier_window.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTranslator>

#include "base/errno_logging.h"
#include "host/host_settings.h"

namespace aspia {

namespace {

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const proto::notifier::Session& session)
        : session_(session)
    {
        switch (session_.session_type())
        {
            case proto::auth::SESSION_TYPE_DESKTOP_MANAGE:
                setIcon(0, QIcon(QStringLiteral(":/icon/monitor-keyboard.png")));
                break;

            case proto::auth::SESSION_TYPE_DESKTOP_VIEW:
                setIcon(0, QIcon(QStringLiteral(":/icon/monitor.png")));
                break;

            case proto::auth::SESSION_TYPE_FILE_TRANSFER:
                setIcon(0, QIcon(QStringLiteral(":/icon/folder-stand.png")));
                break;

            case proto::auth::SESSION_TYPE_SYSTEM_INFO:
                setIcon(0, QIcon(QStringLiteral(":/icon/system-monitor.png")));
                break;

            default:
                qFatal("Unexpected session type: %d", session_.session_type());
                return;
        }

        setText(0, QString("%1 (%2)")
                .arg(QString::fromStdString(session_.username()))
                .arg(QString::fromStdString(session_.remote_address())));
    }

    const proto::notifier::Session& session() const { return session_; }

private:
    proto::notifier::Session session_;
    Q_DISABLE_COPY(SessionTreeItem)
};

} // namespace

HostNotifierWindow::HostNotifierWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    HostSettings settings;

    QString current_locale = settings.locale();

    if (!locale_loader_.contains(current_locale))
    {
        current_locale = HostSettings::defaultLocale();
        settings.setLocale(current_locale);
    }

    locale_loader_.installTranslators(current_locale);
    ui.setupUi(this);

    ui.label_title->installEventFilter(this);
    ui.label_connections->installEventFilter(this);

    connect(ui.button_show_hide, &QPushButton::pressed,
            this, &HostNotifierWindow::onShowHidePressed);

    connect(ui.button_disconnect_all, &QPushButton::pressed,
            this, &HostNotifierWindow::onDisconnectAllPressed);

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &HostNotifierWindow::onContextMenu);

    setAttribute(Qt::WA_TranslucentBackground);

#if defined(Q_OS_WIN)
    taskbar_create_message_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (!taskbar_create_message_)
    {
        qWarningErrno("RegisterWindowMessageW failed");
    }
#endif
}

void HostNotifierWindow::setChannelId(const QString& channel_id)
{
    channel_id_ = channel_id;
}

void HostNotifierWindow::sessionOpen(const proto::notifier::Session& session)
{
    ui.tree->addTopLevelItem(new SessionTreeItem(session));
    ui.button_disconnect_all->setEnabled(true);
}

void HostNotifierWindow::sessionClose(const proto::notifier::SessionClose& session_close)
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = dynamic_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item && item->session().uuid() == session_close.uuid())
        {
            delete item;
            break;
        }
    }

    if (!ui.tree->topLevelItemCount())
        quit();
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

bool HostNotifierWindow::nativeEvent(const QByteArray& event_type, void* message, long* result)
{
#if defined(Q_OS_WIN)
    MSG* native_message = reinterpret_cast<MSG*>(message);
    if (native_message->message == taskbar_create_message_)
    {
        QSize screen_size = QApplication::primaryScreen()->availableSize();
        QSize window_size = frameSize();

        move(screen_size.width() - window_size.width(),
             screen_size.height() - window_size.height());
    }
#endif

    return QWidget::nativeEvent(event_type, message, result);
}

void HostNotifierWindow::showEvent(QShowEvent* event)
{
    if (notifier_.isNull())
    {
        notifier_ = new HostNotifier(this);

        connect(notifier_, &HostNotifier::finished, this, &HostNotifierWindow::quit);
        connect(notifier_, &HostNotifier::sessionOpen, this, &HostNotifierWindow::sessionOpen);
        connect(notifier_, &HostNotifier::sessionClose, this, &HostNotifierWindow::sessionClose);
        connect(this, &HostNotifierWindow::killSession, notifier_, &HostNotifier::killSession);

        if (!notifier_->start(channel_id_))
        {
            quit();
            return;
        }
    }

    QWidget::showEvent(event);
}

void HostNotifierWindow::quit()
{
    close();
    QApplication::quit();
}

void HostNotifierWindow::onShowHidePressed()
{
    if (ui.content->isVisible())
    {
        QSize screen_size = QApplication::primaryScreen()->availableSize();
        QSize content_size = ui.content->frameSize();
        window_rect_ = frameGeometry();

        ui.content->hide();
        ui.title->hide();

        move(screen_size.width() - ui.button_show_hide->width(), pos().y());
        setFixedSize(window_rect_.width() - content_size.width(),
                     window_rect_.height());

        ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/icon/arrow-right-gray.png")));
    }
    else
    {
        ui.content->show();
        ui.title->show();

        move(window_rect_.topLeft());
        setFixedSize(window_rect_.size());

        ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/icon/arrow-left-gray.png")));
    }
}

void HostNotifierWindow::onDisconnectAllPressed()
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = dynamic_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item)
            emit killSession(item->session().uuid());
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

    if (menu.exec(ui.tree->mapToGlobal(point)) == &disconnect_action)
        emit killSession(item->session().uuid());
}

} // namespace aspia
